#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "flowpipe/bounded_queue.h"
#include "flowpipe/payload.h"
#include "flowpipe/queue_runtime.h"
#include "flowpipe/stage.h"
#include "flowpipe/stage_metrics.h"
#include "flowpipe/stage_runner.h"
#include "flowpipe/v1/flow.pb.h"

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

  void RecordStageError(const char*) noexcept override { ++error_calls; }

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

  std::string name() const override { return "fake_source"; }

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
  std::string name() const override { return "fake_transform"; }

  void process(StageContext&, const Payload& input, Payload& output) override {
    seen_inputs.push_back(input.meta);
    output = input;
  }

  std::vector<PayloadMeta> seen_inputs;
};

QueueRuntime MakeQueueRuntime(const std::string& name, uint32_t capacity,
                              flowpipe::v1::QueueType type) {
  return QueueRuntime{
      .name = name,
      .type = type,
      .capacity = capacity,
      .queue = std::make_shared<BoundedQueue<Payload>>(capacity),
  };
}

TEST(RunSourceStageTest, EnqueuesPayloadsAndRecordsMetrics) {
  auto output = MakeQueueRuntime("out", 4, flowpipe::v1::QUEUE_TYPE_BUFFERED);
  std::atomic<bool> stop_flag{false};
  StageContext ctx{StopToken(&stop_flag)};

  std::vector<Payload> payloads(2);
  FakeSourceStage stage(std::move(payloads));
  RecordingStageMetrics metrics;

  RunSourceStage(&stage, ctx, output, &metrics);

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

TEST(RunSourceStageTest, RespectsStopTokenAndClosesQueue) {
  auto output = MakeQueueRuntime("out", 2, flowpipe::v1::QUEUE_TYPE_BUFFERED);
  std::atomic<bool> stop_flag{true};
  StageContext ctx{StopToken(&stop_flag)};

  FakeSourceStage stage({Payload{}});
  RecordingStageMetrics metrics;

  RunSourceStage(&stage, ctx, output, &metrics);

  EXPECT_EQ(metrics.queue_enqueues, 0);
  EXPECT_EQ(metrics.latency_calls, 0);
  EXPECT_FALSE(output.queue->pop(ctx.stop).has_value());
}

TEST(RunTransformStageTest, DequeuesTransformsAndRecordsMetrics) {
  auto input = MakeQueueRuntime("in", 2, flowpipe::v1::QUEUE_TYPE_BUFFERED);
  auto output = MakeQueueRuntime("out", 2, flowpipe::v1::QUEUE_TYPE_BUFFERED);

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

TEST(RunTransformStageTest, StopsWhenCancelledBeforeWork) {
  auto input = MakeQueueRuntime("in", 1, flowpipe::v1::QUEUE_TYPE_BUFFERED);
  auto output = MakeQueueRuntime("out", 1, flowpipe::v1::QUEUE_TYPE_BUFFERED);

  std::atomic<bool> stop_flag{true};
  StageContext ctx{StopToken(&stop_flag)};

  FakeTransformStage stage;
  RecordingStageMetrics metrics;

  RunTransformStage(&stage, ctx, input, output, &metrics);

  EXPECT_EQ(metrics.queue_dequeues, 0);
  EXPECT_EQ(metrics.queue_enqueues, 0);
  EXPECT_EQ(metrics.latency_calls, 0);
  EXPECT_FALSE(output.queue->pop(ctx.stop).has_value());
}

}  // namespace
}  // namespace flowpipe
