#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>

#include "flowpipe/payload.h"
#include "flowpipe/queue.h"

namespace flowpipe {

class DurableQueue final : public IQueue<Payload> {
 public:
  DurableQueue(std::size_t capacity, std::string path)
      : capacity_(capacity), path_(std::move(path)) {
    LoadFromDisk();
  }

  bool push(Payload item, const StopToken& stop) override {
    std::unique_lock lock(mu_);
    not_full_.wait(lock,
                   [&] { return stop.stop_requested() || closed_ || queue_.size() < capacity_; });

    if (stop.stop_requested() || closed_) {
      return false;
    }

    uint64_t record_bytes = RecordSize(item);
    if (!AppendToDiskLocked(item)) {
      return false;
    }

    queue_.push_back(QueueItem{std::move(item), record_bytes});
    not_empty_.notify_one();
    return true;
  }

  std::optional<Payload> pop(const StopToken& stop) override {
    std::unique_lock lock(mu_);
    not_empty_.wait(lock, [&] { return stop.stop_requested() || closed_ || !queue_.empty(); });

    if (!queue_.empty()) {
      QueueItem front = std::move(queue_.front());
      queue_.pop_front();
      head_offset_ += front.record_bytes;
      UpdateHeaderLocked();
      MaybeCompactLocked();
      not_full_.notify_one();
      return std::move(front.payload);
    }
    return std::nullopt;
  }

  void close() override {
    std::lock_guard lock(mu_);
    closed_ = true;
    UpdateHeaderLocked();
    not_empty_.notify_all();
    not_full_.notify_all();
  }

 private:
  struct QueueItem {
    Payload payload;
    uint64_t record_bytes = 0;
  };

  struct FileHeader {
    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t head_offset = 0;
  };

  struct DiskHeader {
    uint64_t payload_size = 0;
    uint64_t enqueue_ts_ns = 0;
    uint32_t flags = 0;
    uint32_t schema_id_size = 0;
    uint8_t trace_id[PayloadMeta::trace_id_size]{};
    uint8_t span_id[PayloadMeta::span_id_size]{};
  };

  static constexpr uint32_t kFileMagic = 0x31515046;  // "FPQ1"
  static constexpr uint32_t kFileVersion = 1;
  static constexpr uint64_t kHeaderSize = sizeof(FileHeader);
  static constexpr uint64_t kCompactThresholdBytes = 4 * 1024 * 1024;

  uint64_t RecordSize(const Payload& payload) const {
    return sizeof(DiskHeader) + payload.meta.schema_id.size() + payload.size;
  }

  void LoadFromDisk() {
    if (path_.empty()) {
      return;
    }

    std::ifstream input(path_, std::ios::binary);
    if (!input.good()) {
      return;
    }

    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size <= 0) {
      head_offset_ = kHeaderSize;
      file_size_ = 0;
      return;
    }

    input.seekg(0);
    FileHeader header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    const bool has_header =
        input.good() && header.magic == kFileMagic && header.version == kFileVersion;
    if (has_header) {
      head_offset_ = header.head_offset;
      file_size_ = static_cast<uint64_t>(size);
      LoadFromStream(input, head_offset_);
      return;
    }

    input.clear();
    input.seekg(0);
    LoadLegacyFromStream(input);
    RewriteFileFromQueue();
  }

  void LoadFromStream(std::istream& input, uint64_t offset) {
    if (offset < kHeaderSize) {
      offset = kHeaderSize;
    }

    input.seekg(static_cast<std::streamoff>(offset));
    while (input.good()) {
      DiskHeader header{};
      input.read(reinterpret_cast<char*>(&header), sizeof(header));
      if (!input.good()) {
        break;
      }

      const bool should_store = queue_.size() < capacity_;
      std::string schema_id;
      if (header.schema_id_size > 0) {
        if (should_store) {
          schema_id.resize(header.schema_id_size);
          input.read(schema_id.data(), header.schema_id_size);
        } else {
          input.seekg(static_cast<std::streamoff>(header.schema_id_size), std::ios::cur);
        }
        if (!input.good()) {
          break;
        }
      }

      std::shared_ptr<uint8_t[]> buffer;
      if (header.payload_size > 0) {
        if (should_store) {
          buffer = AllocatePayloadBuffer(static_cast<size_t>(header.payload_size));
          if (!buffer) {
            break;
          }
          input.read(reinterpret_cast<char*>(buffer.get()),
                     static_cast<std::streamsize>(header.payload_size));
        } else {
          input.seekg(static_cast<std::streamoff>(header.payload_size), std::ios::cur);
        }
        if (!input.good()) {
          break;
        }
      }

      if (!should_store) {
        continue;
      }

      PayloadMeta meta{};
      meta.enqueue_ts_ns = header.enqueue_ts_ns;
      meta.flags = header.flags;
      std::memcpy(meta.trace_id, header.trace_id, PayloadMeta::trace_id_size);
      std::memcpy(meta.span_id, header.span_id, PayloadMeta::span_id_size);
      meta.schema_id = std::move(schema_id);

      queue_.push_back(QueueItem{
          Payload(std::move(buffer), static_cast<size_t>(header.payload_size), std::move(meta)),
          sizeof(DiskHeader) + header.schema_id_size + header.payload_size});
    }
  }

  void LoadLegacyFromStream(std::istream& input) {
    while (input.good()) {
      DiskHeader header{};
      input.read(reinterpret_cast<char*>(&header), sizeof(header));
      if (!input.good()) {
        break;
      }

      std::string schema_id;
      if (header.schema_id_size > 0) {
        schema_id.resize(header.schema_id_size);
        input.read(schema_id.data(), header.schema_id_size);
        if (!input.good()) {
          break;
        }
      }

      auto buffer = AllocatePayloadBuffer(static_cast<size_t>(header.payload_size));
      if (!buffer) {
        break;
      }

      if (header.payload_size > 0) {
        input.read(reinterpret_cast<char*>(buffer.get()),
                   static_cast<std::streamsize>(header.payload_size));
        if (!input.good()) {
          break;
        }
      }

      PayloadMeta meta{};
      meta.enqueue_ts_ns = header.enqueue_ts_ns;
      meta.flags = header.flags;
      std::memcpy(meta.trace_id, header.trace_id, PayloadMeta::trace_id_size);
      std::memcpy(meta.span_id, header.span_id, PayloadMeta::span_id_size);
      meta.schema_id = std::move(schema_id);

      if (queue_.size() >= capacity_) {
        continue;
      }

      queue_.push_back(QueueItem{
          Payload(std::move(buffer), static_cast<size_t>(header.payload_size), std::move(meta)),
          sizeof(DiskHeader) + header.schema_id_size + header.payload_size});
    }
  }

  bool EnsureFileReadyLocked() {
    if (path_.empty()) {
      return false;
    }

    if (file_.is_open()) {
      return true;
    }

    file_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!file_.is_open()) {
      std::ofstream create(path_, std::ios::binary | std::ios::trunc);
      if (!create.good()) {
        return false;
      }
      FileHeader header{kFileMagic, kFileVersion, kHeaderSize};
      create.write(reinterpret_cast<const char*>(&header), sizeof(header));
      create.flush();
      create.close();
      file_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
    }

    if (!file_.is_open()) {
      return false;
    }

    file_.clear();
    file_.seekg(0, std::ios::end);
    file_size_ = static_cast<uint64_t>(file_.tellg());
    if (file_size_ == 0) {
      FileHeader header{kFileMagic, kFileVersion, kHeaderSize};
      file_.seekp(0);
      file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
      file_.flush();
      file_size_ = kHeaderSize;
      head_offset_ = kHeaderSize;
    }

    return file_.good();
  }

  bool AppendToDiskLocked(const Payload& payload) {
    if (path_.empty()) {
      return true;
    }

    if (!EnsureFileReadyLocked()) {
      return false;
    }

    DiskHeader header{};
    header.payload_size = payload.size;
    header.enqueue_ts_ns = payload.meta.enqueue_ts_ns;
    header.flags = payload.meta.flags;
    header.schema_id_size = static_cast<uint32_t>(payload.meta.schema_id.size());
    std::memcpy(header.trace_id, payload.meta.trace_id, PayloadMeta::trace_id_size);
    std::memcpy(header.span_id, payload.meta.span_id, PayloadMeta::span_id_size);

    file_.clear();
    file_.seekp(0, std::ios::end);
    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!payload.meta.schema_id.empty()) {
      file_.write(payload.meta.schema_id.data(),
                  static_cast<std::streamsize>(payload.meta.schema_id.size()));
    }
    if (payload.size > 0 && payload.buffer) {
      file_.write(reinterpret_cast<const char*>(payload.data()),
                  static_cast<std::streamsize>(payload.size));
    }
    file_.flush();

    if (!file_.good()) {
      return false;
    }

    file_size_ = static_cast<uint64_t>(file_.tellp());
    return true;
  }

  void UpdateHeaderLocked() {
    if (path_.empty()) {
      return;
    }

    if (!EnsureFileReadyLocked()) {
      return;
    }

    if (head_offset_ < kHeaderSize) {
      head_offset_ = kHeaderSize;
    }
    if (file_size_ > 0 && head_offset_ > file_size_) {
      head_offset_ = file_size_;
    }

    FileHeader header{kFileMagic, kFileVersion, head_offset_};
    file_.clear();
    file_.seekp(0);
    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file_.flush();
  }

  void MaybeCompactLocked() {
    if (path_.empty()) {
      return;
    }

    if (head_offset_ < kHeaderSize) {
      return;
    }

    if (head_offset_ < kCompactThresholdBytes || head_offset_ < (file_size_ / 2)) {
      return;
    }

    RewriteFileFromQueue();
  }

  void RewriteFileFromQueue() {
    if (path_.empty()) {
      return;
    }

    const std::filesystem::path target(path_);
    const std::filesystem::path temp = target;
    const std::filesystem::path temp_path = temp.string() + ".tmp";

    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output.good()) {
      return;
    }

    FileHeader header{kFileMagic, kFileVersion, kHeaderSize};
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));

    for (const auto& item : queue_) {
      const auto& payload = item.payload;
      DiskHeader record{};
      record.payload_size = payload.size;
      record.enqueue_ts_ns = payload.meta.enqueue_ts_ns;
      record.flags = payload.meta.flags;
      record.schema_id_size = static_cast<uint32_t>(payload.meta.schema_id.size());
      std::memcpy(record.trace_id, payload.meta.trace_id, PayloadMeta::trace_id_size);
      std::memcpy(record.span_id, payload.meta.span_id, PayloadMeta::span_id_size);

      output.write(reinterpret_cast<const char*>(&record), sizeof(record));
      if (!payload.meta.schema_id.empty()) {
        output.write(payload.meta.schema_id.data(),
                     static_cast<std::streamsize>(payload.meta.schema_id.size()));
      }
      if (payload.size > 0 && payload.buffer) {
        output.write(reinterpret_cast<const char*>(payload.data()),
                     static_cast<std::streamsize>(payload.size));
      }
    }

    output.flush();
    std::error_code error;
    if (file_.is_open()) {
      file_.close();
    }
    std::filesystem::rename(temp_path, target, error);
    if (error) {
      std::filesystem::remove(temp_path, error);
      return;
    }

    head_offset_ = kHeaderSize;
    file_size_ = static_cast<uint64_t>(std::filesystem::file_size(target, error));
    file_.open(path_, std::ios::binary | std::ios::in | std::ios::out);
  }

  std::size_t capacity_;
  std::string path_;
  std::mutex mu_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  std::deque<QueueItem> queue_;
  std::fstream file_;
  uint64_t head_offset_{kHeaderSize};
  uint64_t file_size_{0};
  bool closed_{false};
};

}  // namespace flowpipe
