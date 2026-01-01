#include "flowpipe/stage_runner.h"

#include <chrono>

namespace flowpipe {

// ------------------------------------------------------------
// Time helper (monotonic, nanoseconds)
// ------------------------------------------------------------
static inline uint64_t now_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// ------------------------------------------------------------
// Source stage runner
// ------------------------------------------------------------
void RunSourceStage(ISourceStage* stage, StageContext& ctx, QueueRuntime& output,
                    StageMetrics* metrics) {
  // Source stages typically push into the output queue themselves,
  // so we only measure stage execution time and enqueue metrics
  while (!ctx.stop.stop_requested()) {
    uint64_t start_ns = now_ns();

    // Let the source stage produce into the queue
    stage->run(ctx, *output.queue);

    uint64_t end_ns = now_ns();

    if (metrics) {
      metrics->RecordStageLatency(stage->name().c_str(), end_ns - start_ns);
    }

    // NOTE:
    // Enqueue metrics are usually recorded at the actual push site.
    // If you centralize enqueue in the runtime later, hook it here.
  }
}

// ------------------------------------------------------------
// Transform stage runner
// ------------------------------------------------------------
void RunTransformStage(ITransformStage* stage, StageContext& ctx, QueueRuntime& input,
                       QueueRuntime& output, StageMetrics* metrics) {
  while (!ctx.stop.stop_requested()) {
    auto item = input.queue->pop(ctx.stop);
    if (!item.has_value()) {
      break;
    }

    const Payload& payload = *item;

    // ------------------------------
    // Queue dequeue metrics
    // ------------------------------
    if (metrics) {
      metrics->RecordQueueDequeue(input, payload);
    }

    // ------------------------------
    // Stage execution latency
    // ------------------------------
    uint64_t start_ns = now_ns();

    stage->run(ctx, *input.queue, *output.queue);

    uint64_t end_ns = now_ns();

    if (metrics) {
      metrics->RecordStageLatency(stage->name().c_str(), end_ns - start_ns);
    }

    // NOTE:
    // Enqueue metrics for `output` should be recorded
    // where the payload is actually pushed.
  }
}

// ------------------------------------------------------------
// Sink stage runner
// ------------------------------------------------------------
void RunSinkStage(ISinkStage* stage, StageContext& ctx, QueueRuntime& input,
                  StageMetrics* metrics) {
  while (!ctx.stop.stop_requested()) {
    auto item = input.queue->pop(ctx.stop);
    if (!item.has_value()) {
      break;
    }

    const Payload& payload = *item;

    // ------------------------------
    // Queue dequeue metrics
    // ------------------------------
    if (metrics) {
      metrics->RecordQueueDequeue(input, payload);
    }

    // ------------------------------
    // Stage execution latency
    // ------------------------------
    uint64_t start_ns = now_ns();

    stage->run(ctx, *input.queue);

    uint64_t end_ns = now_ns();

    if (metrics) {
      metrics->RecordStageLatency(stage->name().c_str(), end_ns - start_ns);
    }
  }
}

}  // namespace flowpipe
