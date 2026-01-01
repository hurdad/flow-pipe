#pragma once

#include <cstdint>

namespace flowpipe {

// Forward declarations only — no heavy includes here
struct Payload;
struct QueueRuntime;

/**
 * Runtime-owned metrics facade for stages and queues.
 *
 * This class:
 *  - hides OpenTelemetry details
 *  - is safe to call from hot paths
 *  - may be a no-op if metrics are disabled
 *
 * Stages never include or depend on this.
 */
class StageMetrics {
 public:
  StageMetrics() = default;
  ~StageMetrics() = default;

  StageMetrics(const StageMetrics&) = delete;
  StageMetrics& operator=(const StageMetrics&) = delete;

  // ------------------------------------------------------------
  // Queue metrics
  // ------------------------------------------------------------

  // Called when a payload is dequeued from a queue
  void RecordQueueDequeue(const QueueRuntime& queue, const Payload& payload) noexcept;

  // Called when a payload is enqueued into a queue
  void RecordQueueEnqueue(const QueueRuntime& queue) noexcept;

  // ------------------------------------------------------------
  // Stage metrics
  // ------------------------------------------------------------

  // Called after a stage processes a payload
  void RecordStageLatency(const char* stage_name, uint64_t latency_ns) noexcept;

  // Called when a stage reports an error
  void RecordStageError(const char* stage_name) noexcept;
};

}  // namespace flowpipe
