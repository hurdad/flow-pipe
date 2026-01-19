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

    queue_.push_back(std::move(item));
    PersistToDiskLocked();
    not_empty_.notify_one();
    return true;
  }

  std::optional<Payload> pop(const StopToken& stop) override {
    std::unique_lock lock(mu_);
    not_empty_.wait(lock, [&] { return stop.stop_requested() || closed_ || !queue_.empty(); });

    if (!queue_.empty()) {
      Payload item = std::move(queue_.front());
      queue_.pop_front();
      PersistToDiskLocked();
      not_full_.notify_one();
      return item;
    }
    return std::nullopt;
  }

  void close() override {
    std::lock_guard lock(mu_);
    closed_ = true;
    PersistToDiskLocked();
    not_empty_.notify_all();
    not_full_.notify_all();
  }

 private:
  struct DiskHeader {
    uint64_t payload_size = 0;
    uint64_t enqueue_ts_ns = 0;
    uint32_t flags = 0;
    uint32_t schema_id_size = 0;
    uint8_t trace_id[PayloadMeta::trace_id_size]{};
    uint8_t span_id[PayloadMeta::span_id_size]{};
  };

  void LoadFromDisk() {
    if (path_.empty()) {
      return;
    }

    std::ifstream input(path_, std::ios::binary);
    if (!input.good()) {
      return;
    }

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

      queue_.emplace_back(std::move(buffer), static_cast<size_t>(header.payload_size),
                          std::move(meta));
    }
  }

  void PersistToDiskLocked() {
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

    for (const auto& payload : queue_) {
      DiskHeader header{};
      header.payload_size = payload.size;
      header.enqueue_ts_ns = payload.meta.enqueue_ts_ns;
      header.flags = payload.meta.flags;
      header.schema_id_size = static_cast<uint32_t>(payload.meta.schema_id.size());
      std::memcpy(header.trace_id, payload.meta.trace_id, PayloadMeta::trace_id_size);
      std::memcpy(header.span_id, payload.meta.span_id, PayloadMeta::span_id_size);

      output.write(reinterpret_cast<const char*>(&header), sizeof(header));
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
    std::filesystem::rename(temp_path, target, error);
    if (error) {
      std::filesystem::remove(temp_path, error);
    }
  }

  std::size_t capacity_;
  std::string path_;
  std::mutex mu_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  std::deque<Payload> queue_;
  bool closed_{false};
};

}  // namespace flowpipe
