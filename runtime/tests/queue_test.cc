#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>

#include "flowpipe/bounded_queue.h"
#include "flowpipe/durable_queue.h"
#include "flowpipe/payload.h"
#include "flowpipe/stop_token.h"

namespace flowpipe {
namespace {

struct TempQueueFile {
  explicit TempQueueFile(const std::string& suffix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    path = std::filesystem::temp_directory_path() /
           ("flowpipe_queue_test_" + std::to_string(now) + suffix);
  }

  ~TempQueueFile() {
    std::error_code error;
    std::filesystem::remove(path, error);
  }

  std::filesystem::path path;
};

Payload BuildPayload(const std::string& data, const PayloadMeta& meta) {
  auto buffer = AllocatePayloadBuffer(data.size());
  if (!buffer && !data.empty()) {
    return {};
  }
  if (!data.empty()) {
    std::memcpy(buffer.get(), data.data(), data.size());
  }
  return Payload{std::move(buffer), data.size(), meta};
}

}  // namespace

TEST(BoundedQueueTest, PushAndPop) {
  std::atomic<bool> stop_flag{false};
  StopToken stop{&stop_flag};

  BoundedQueue<int> queue(2);

  EXPECT_TRUE(queue.push(42, stop));
  auto item = queue.pop(stop);

  ASSERT_TRUE(item.has_value());
  EXPECT_EQ(*item, 42);
}

TEST(DurableQueueTest, PersistsPayloadsAcrossInstances) {
  std::atomic<bool> stop_flag{false};
  StopToken stop{&stop_flag};

  TempQueueFile temp_file(".bin");
  PayloadMeta meta{};
  meta.enqueue_ts_ns = 123;
  meta.flags = 7;
  meta.schema_id = "schema";
  std::memset(meta.trace_id, 0xAB, PayloadMeta::trace_id_size);
  std::memset(meta.span_id, 0xCD, PayloadMeta::span_id_size);

  {
    DurableQueue queue(4, temp_file.path.string());
    auto payload = BuildPayload("data", meta);
    ASSERT_FALSE(payload.empty());
    EXPECT_TRUE(queue.push(std::move(payload), stop));
  }

  {
    DurableQueue queue(4, temp_file.path.string());
    auto item = queue.pop(stop);
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(item->size, 4u);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(item->data()), item->size), "data");
    EXPECT_EQ(item->meta.enqueue_ts_ns, 123u);
    EXPECT_EQ(item->meta.flags, 7u);
    EXPECT_EQ(item->meta.schema_id, "schema");
    EXPECT_EQ(item->meta.trace_id[0], static_cast<uint8_t>(0xAB));
    EXPECT_EQ(item->meta.span_id[0], static_cast<uint8_t>(0xCD));
  }
}

TEST(DurableQueueTest, CompactsOnHeadAdvance) {
  std::atomic<bool> stop_flag{false};
  StopToken stop{&stop_flag};

  TempQueueFile temp_file(".bin");
  constexpr size_t kPayloadSize = 5 * 1024 * 1024;
  std::string data(kPayloadSize, 'x');
  PayloadMeta meta{};

  {
    DurableQueue queue(3, temp_file.path.string());
    auto payload1 = BuildPayload(data, meta);
    auto payload2 = BuildPayload(data, meta);
    ASSERT_FALSE(payload1.empty());
    ASSERT_FALSE(payload2.empty());
    EXPECT_TRUE(queue.push(std::move(payload1), stop));
    EXPECT_TRUE(queue.push(std::move(payload2), stop));
  }

  uintmax_t size_before = std::filesystem::file_size(temp_file.path);
  ASSERT_GT(size_before, 0u);

  {
    DurableQueue queue(3, temp_file.path.string());
    auto item = queue.pop(stop);
    ASSERT_TRUE(item.has_value());
  }

  uintmax_t size_after = std::filesystem::file_size(temp_file.path);
  EXPECT_GT(size_after, 0u);
  EXPECT_LT(size_after, size_before);
}

}  // namespace flowpipe
