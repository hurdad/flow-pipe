#include "flowpipe/stage_runner.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <cstdint>
#include <mutex>
#include <memory>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>

#include "flowpipe/bounded_queue.h"
#include "flowpipe/payload.h"
#include "flowpipe/queue_runtime.h"
#include "flowpipe/stage.h"
#include "flowpipe/stage_metrics.h"

namespace flowpipe {
namespace {

class RecordingStageMetrics : public StageMetrics {
 public:
  void RecordQueueDequeue(const QueueRuntime& queue, const Payload& payload) noexcept override {
    ++queue_dequeues;
    last_queue_name = queue.name;
    last_dequeue_meta = payload.meta;
  }

  void RecordQueueEnqueue(const QueueRuntime& queue) noexcept override {
    ++queue_enqueues;
    last_queue_name = queue.name;
  }

  void RecordStageLatency(const char*, uint64_t latency_ns) noexcept override {
    ++latency_calls;
    last_latency = latency_ns;
  }

  void RecordStageError(const char*) noexcept override {
    ++error_calls;
  }

  int queue_dequeues = 0;
  int queue_enqueues = 0;
  int latency_calls = 0;
  int error_calls = 0;
  uint64_t last_latency = 0;
  std::string last_queue_name;
  PayloadMeta last_dequeue_meta{};
};

class FakeSourceStage : public ISourceStage {
 public:
  explicit FakeSourceStage(std::vector<Payload> payloads) : payloads_(std::move(payloads)) {}

  std::string name() const override {
    return "fake_source";
  }

  bool produce(StageContext&, Payload& out) override {
    if (index_ >= payloads_.size()) {
      return false;
    }
    out = payloads_[index_++];
    return true;
  }

 private:
  std::vector<Payload> payloads_;
  std::size_t index_ = 0;
};

class FakeTransformStage : public ITransformStage {
 public:
  std::string name() const override {
    return "fake_transform";
  }

  void process(StageContext&, const Payload& input, Payload& output) override {
    seen_inputs.push_back(input.meta);
    output = input;
  }

  std::vector<PayloadMeta> seen_inputs;
};

QueueRuntime MakeQueueRuntime(const std::string& name, uint32_t capacity,
                              std::string schema_id = {}) {
  return QueueRuntime{
      .name = name,
      .capacity = capacity,
      .queue = std::make_shared<BoundedQueue<Payload>>(capacity),
      .schema_id = std::move(schema_id),
  };
}

TEST(RunSourceStageTest, EnqueuesPayloadsAndRecordsMetrics) {
  auto output = MakeQueueRuntime("out", 4);
  std::atomic<bool> stop_flag{false};
  StageContext ctx{StopToken(&stop_flag)};

  std::vector<Payload> payloads(2);
  FakeSourceStage stage(std::move(payloads));
  RecordingStageMetrics metrics;

  RunSourceStage(&stage, ctx, output, &metrics);
  output.queue->close();

  auto first = output.queue->pop(ctx.stop);
  auto second = output.queue->pop(ctx.stop);
  auto closed = output.queue->pop(ctx.stop);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_FALSE(closed.has_value());

  EXPECT_EQ(metrics.queue_enqueues, 2);
  EXPECT_EQ(metrics.latency_calls, 2);
  EXPECT_GT(first->meta.enqueue_ts_ns, 0u);
  EXPECT_GT(second->meta.enqueue_ts_ns, 0u);
}

TEST(RunSourceStageTest, AppliesQueueSchemaIdToPayloads) {
  auto output = MakeQueueRuntime("out", 2, "schema-1");
  std::atomic<bool> stop_flag{false};
  StageContext ctx{StopToken(&stop_flag)};

  std::vector<Payload> payloads(1);
  FakeSourceStage stage(std::move(payloads));
  RecordingStageMetrics metrics;

  RunSourceStage(&stage, ctx, output, &metrics);
  output.queue->close();

  auto first = output.queue->pop(ctx.stop);
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->meta.schema_id, "schema-1");
}

TEST(RunSourceStageTest, RespectsStopTokenAndClosesQueue) {
  auto output = MakeQueueRuntime("out", 2);
  std::atomic<bool> stop_flag{true};
  StageContext ctx{StopToken(&stop_flag)};

  FakeSourceStage stage({Payload{}});
  RecordingStageMetrics metrics;

  RunSourceStage(&stage, ctx, output, &metrics);

  output.queue->close();

  EXPECT_EQ(metrics.queue_enqueues, 0);
  EXPECT_EQ(metrics.latency_calls, 0);
  EXPECT_FALSE(output.queue->pop(ctx.stop).has_value());
}

TEST(RunTransformStageTest, DequeuesTransformsAndRecordsMetrics) {
  auto input = MakeQueueRuntime("in", 2);
  auto output = MakeQueueRuntime("out", 2);

  Payload input_payload;
  input_payload.meta.trace_id[0] = 0xAA;
  input_payload.meta.flags = 3;
  input_payload.meta.enqueue_ts_ns = 123;

  std::atomic<bool> stop_flag{false};
  StageContext ctx{StopToken(&stop_flag)};

  ASSERT_TRUE(input.queue->push(input_payload, ctx.stop));
  input.queue->close();

  FakeTransformStage stage;
  RecordingStageMetrics metrics;

  RunTransformStage(&stage, ctx, input, output, &metrics);
  output.queue->close();

  auto out_payload = output.queue->pop(ctx.stop);
  auto closed = output.queue->pop(ctx.stop);

  ASSERT_TRUE(out_payload.has_value());
  EXPECT_FALSE(closed.has_value());
  EXPECT_EQ(metrics.queue_dequeues, 1);
  EXPECT_EQ(metrics.queue_enqueues, 1);
  EXPECT_EQ(metrics.latency_calls, 1);
  EXPECT_EQ(metrics.last_dequeue_meta.flags, 3u);
  EXPECT_EQ(out_payload->meta.flags, 3u);
  EXPECT_EQ(out_payload->meta.trace_id[0], 0xAA);
  EXPECT_GT(out_payload->meta.enqueue_ts_ns, 0u);
  ASSERT_EQ(stage.seen_inputs.size(), 1u);
  EXPECT_EQ(stage.seen_inputs.back().trace_id[0], 0xAA);
}

TEST(RunTransformStageTest, DropsPayloadsWithSchemaMismatch) {
  auto input = MakeQueueRuntime("in", 1, "schema-a");
  auto output = MakeQueueRuntime("out", 1, "schema-b");

  Payload input_payload;
  input_payload.meta.schema_id = "schema-wrong";

  std::atomic<bool> stop_flag{false};
  StageContext ctx{StopToken(&stop_flag)};

  ASSERT_TRUE(input.queue->push(input_payload, ctx.stop));
  input.queue->close();

  FakeTransformStage stage;
  RecordingStageMetrics metrics;

  RunTransformStage(&stage, ctx, input, output, &metrics);

  output.queue->close();

  auto out_payload = output.queue->pop(ctx.stop);
  EXPECT_FALSE(out_payload.has_value());
  EXPECT_EQ(metrics.error_calls, 1);
}


class ThrowingTransformStage : public ITransformStage {
 public:
  std::string name() const override {
    return "throwing_transform";
  }

  void process(StageContext&, const Payload&, Payload&) override {
    throw std::runtime_error("boom");
  }
};

TEST(RunTransformStageTest, WorkerExceptionRequestsGlobalStopAndUnblocksPeers) {
  auto input = MakeQueueRuntime("in", 1);
  auto output = MakeQueueRuntime("out", 1);

  std::atomic<bool> stop_flag{false};
  StageContext ctx{StopToken(&stop_flag)};

  ASSERT_TRUE(input.queue->push(Payload{}, ctx.stop));

  ThrowingTransformStage first_worker;
  ThrowingTransformStage second_worker;

  auto run_worker = [&](ITransformStage* stage) {
    RunTransformStage(stage, ctx, input, output, nullptr);
  };

  auto first = std::async(std::launch::async, run_worker, &first_worker);
  auto second = std::async(std::launch::async, run_worker, &second_worker);

  EXPECT_EQ(first.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  EXPECT_EQ(second.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  EXPECT_TRUE(stop_flag.load());
}

TEST(RunTransformStageTest, StopsWhenCancelledBeforeWork) {
  auto input = MakeQueueRuntime("in", 1);
  auto output = MakeQueueRuntime("out", 1);

  std::atomic<bool> stop_flag{true};
  StageContext ctx{StopToken(&stop_flag)};

  FakeTransformStage stage;
  RecordingStageMetrics metrics;

  RunTransformStage(&stage, ctx, input, output, &metrics);

  output.queue->close();

  EXPECT_EQ(metrics.queue_dequeues, 0);
  EXPECT_EQ(metrics.queue_enqueues, 0);
  EXPECT_EQ(metrics.latency_calls, 0);
  EXPECT_FALSE(output.queue->pop(ctx.stop).has_value());
}

class SequencedSourceStage : public ISourceStage {
 public:
  explicit SequencedSourceStage(bool should_wait) : should_wait_(should_wait) {}

  std::string name() const override {
    return "sequenced_source";
  }

  bool produce(StageContext&, Payload& out) override {
    if (!should_wait_) {
      return false;
    }

    {
      std::lock_guard<std::mutex> lock(mu_);
      waiting_ = true;
    }
    cv_.notify_all();

    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [&]() { return released_; });

    out.meta.flags = 7;
    should_wait_ = false;
    return true;
  }

  void WaitUntilWaiting() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [&]() { return waiting_; });
  }

  void Release() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      released_ = true;
    }
    cv_.notify_all();
  }

 private:
  bool should_wait_;
  std::mutex mu_;
  std::condition_variable cv_;
  bool waiting_ = false;
  bool released_ = false;
};

TEST(RuntimeQueueOwnershipTest, SourceWorkersCloseOutputOnlyAfterLastWorkerExits) {
  auto output = MakeQueueRuntime("out", 4);
  std::atomic<bool> stop_flag{false};
  StageContext ctx{StopToken(&stop_flag)};
  auto remaining_workers = std::make_shared<std::atomic<int>>(2);

  SequencedSourceStage exits_early(false);
  SequencedSourceStage long_running(true);

  auto worker = [&](ISourceStage* stage) {
    RunSourceStage(stage, ctx, output, nullptr);
    if (remaining_workers->fetch_sub(1) == 1) {
      output.queue->close();
    }
  };

  std::thread t1(worker, &exits_early);
  std::thread t2(worker, &long_running);

  long_running.WaitUntilWaiting();
  EXPECT_TRUE(output.queue->push(Payload{}, ctx.stop));

  long_running.Release();

  t1.join();
  t2.join();

  int payloads = 0;
  while (output.queue->pop(ctx.stop).has_value()) {
    ++payloads;
  }

  EXPECT_GE(payloads, 2);
}

class ExitAfterOneTransformStage : public ITransformStage {
 public:
  std::string name() const override {
    return "exit_after_one";
  }

  void process(StageContext&, const Payload&, Payload& output) override {
    output.meta.flags = 1;
  }
};

class BlockingTransformStage : public ITransformStage {
 public:
  std::string name() const override {
    return "blocking_transform";
  }

  void process(StageContext&, const Payload&, Payload& output) override {
    {
      std::lock_guard<std::mutex> lock(mu_);
      processing_ = true;
    }
    cv_.notify_all();

    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [&]() { return released_; });
    output.meta.flags = 2;
  }

  void WaitUntilProcessing() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [&]() { return processing_; });
  }

  void Release() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      released_ = true;
    }
    cv_.notify_all();
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  bool processing_ = false;
  bool released_ = false;
};

TEST(RuntimeQueueOwnershipTest, TransformWorkersCloseOutputOnlyAfterLastWorkerExits) {
  auto input = MakeQueueRuntime("in", 4);
  auto output = MakeQueueRuntime("out", 4);
  std::atomic<bool> stop_flag{false};
  StageContext ctx{StopToken(&stop_flag)};
  auto remaining_workers = std::make_shared<std::atomic<int>>(2);

  ExitAfterOneTransformStage exits_early;
  BlockingTransformStage long_running;

  ASSERT_TRUE(input.queue->push(Payload{}, ctx.stop));

  auto worker = [&](ITransformStage* stage) {
    RunTransformStage(stage, ctx, input, output, nullptr);
    if (remaining_workers->fetch_sub(1) == 1) {
      output.queue->close();
    }
  };

  std::thread t2(worker, &long_running);

  long_running.WaitUntilProcessing();
  ASSERT_TRUE(input.queue->push(Payload{}, ctx.stop));
  std::thread t1(worker, &exits_early);
  input.queue->close();

  EXPECT_TRUE(output.queue->push(Payload{}, ctx.stop));

  long_running.Release();

  t1.join();
  t2.join();

  int payloads = 0;
  while (output.queue->pop(ctx.stop).has_value()) {
    ++payloads;
  }

  EXPECT_GE(payloads, 3);
}

TEST(RuntimeQueueOwnershipTest, SharedOutputQueueClosesOnlyAfterAllProducerStagesExit) {
  auto output = MakeQueueRuntime("out", 4);
  std::atomic<bool> stop_flag{false};
  StageContext ctx{StopToken(&stop_flag)};
  auto queue_remaining_producers = std::make_shared<std::atomic<int>>(2);

  SequencedSourceStage exits_early(false);
  SequencedSourceStage long_running(true);

  auto worker = [&](ISourceStage* stage) {
    RunSourceStage(stage, ctx, output, nullptr);
    if (queue_remaining_producers->fetch_sub(1) == 1) {
      output.queue->close();
    }
  };

  std::thread t1(worker, &exits_early);
  std::thread t2(worker, &long_running);

  long_running.WaitUntilWaiting();
  EXPECT_TRUE(output.queue->push(Payload{}, ctx.stop));

  long_running.Release();

  t1.join();
  t2.join();

  int payloads = 0;
  while (output.queue->pop(ctx.stop).has_value()) {
    ++payloads;
  }

  EXPECT_GE(payloads, 2);
}

}  // namespace
}  // namespace flowpipe
