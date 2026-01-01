#include "flowpipe/stage_metrics.h"

#include "flowpipe/payload.h"
#include "flowpipe/queue_runtime.h"

// Existing observability system
#include "flowpipe/observability/metrics.h"
#include "flowpipe/observability/observability_state.h"

namespace flowpipe {

// ------------------------------------------------------------
// Queue metrics
// ------------------------------------------------------------
void StageMetrics::RecordQueueDequeue(const QueueRuntime& queue, const Payload& payload) noexcept {
#if FLOWPIPE_ENABLE_OTEL
  auto& state = observability::GetOtelState();
  if (!state.queue_metrics_enabled) {
    return;
  }
  //
  // auto* metrics = observability::GetMetrics();
  // if (!metrics) {
  //   return;
  // }
  //
  // metrics->RecordQueueDequeue(queue, payload);
#else
  (void)queue;
  (void)payload;
#endif
}

void StageMetrics::RecordQueueEnqueue(const QueueRuntime& queue) noexcept {
#if FLOWPIPE_ENABLE_OTEL
  auto& state = observability::GetOtelState();
  if (!state.queue_metrics_enabled) {
    return;
  }

  // auto* metrics = observability::GetMetrics();
  // if (!metrics) {
  //   return;
  // }
  //
  // metrics->RecordQueueEnqueue(queue);
#else
  (void)queue;
#endif
}

// ------------------------------------------------------------
// Stage metrics
// ------------------------------------------------------------
void StageMetrics::RecordStageLatency(const char* stage_name, uint64_t latency_ns) noexcept {
#if FLOWPIPE_ENABLE_OTEL
  auto& state = observability::GetOtelState();
  if (!state.stage_metrics_enabled) {
    return;
  }

  // auto* metrics = observability::GetMetrics();
  // if (!metrics) {
  //   return;
  // }
  //
  // metrics->RecordStageLatency(stage_name, latency_ns);
#else
  (void)stage_name;
  (void)latency_ns;
#endif
}

void StageMetrics::RecordStageError(const char* stage_name) noexcept {
#if FLOWPIPE_ENABLE_OTEL
  auto& state = observability::GetOtelState();
  if (!state.stage_metrics_enabled) {
    return;
  }

  // auto* metrics = observability::GetMetrics();
  // if (!metrics) {
  //   return;
  // }
  //
  // metrics->RecordStageError(stage_name);
#else
  (void)stage_name;
#endif
}

}  // namespace flowpipe
