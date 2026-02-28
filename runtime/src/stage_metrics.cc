#include "flowpipe/stage_metrics.h"

#include "flowpipe/observability/metrics.h"
#include "flowpipe/observability/observability_state.h"
#include "flowpipe/payload.h"
#include "flowpipe/queue_runtime.h"

#if FLOWPIPE_ENABLE_OTEL
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/metrics/provider.h>

#include <chrono>
#endif

namespace flowpipe {

#if FLOWPIPE_ENABLE_OTEL

// Single meter instance shared by all Record* methods.
static opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter>& GetMeter() {
  static auto meter = opentelemetry::metrics::Provider::GetMeterProvider()->GetMeter(
      "flowpipe.runtime", "1.0.0");
  return meter;
}

static auto& dequeue_counter = *[] {
  static auto c = GetMeter()->CreateUInt64Counter("flowpipe.queue.dequeue.count",
                                                  "Number of records dequeued from queue");
  return &c;
}();
static auto& dwell_histogram = *[] {
  static auto h = GetMeter()->CreateUInt64Histogram("flowpipe.queue.dwell_ns",
                                                    "Time records spent in queue (ns)");
  return &h;
}();
static auto& enqueue_counter = *[] {
  static auto c = GetMeter()->CreateUInt64Counter("flowpipe.queue.enqueue.count",
                                                  "Number of records enqueued to queue");
  return &c;
}();
static auto& process_counter = *[] {
  static auto c = GetMeter()->CreateUInt64Counter("flowpipe.stage.process.count",
                                                  "Number of stage invocations");
  return &c;
}();
static auto& latency_histogram = *[] {
  static auto h = GetMeter()->CreateUInt64Histogram("flowpipe.stage.latency_ns",
                                                    "Stage processing latency (ns)");
  return &h;
}();
static auto& error_counter = *[] {
  static auto c = GetMeter()->CreateUInt64Counter("flowpipe.stage.errors",
                                                  "Number of stage errors");
  return &c;
}();

#endif  // FLOWPIPE_ENABLE_OTEL

// ------------------------------------------------------------
// Queue metrics
// ------------------------------------------------------------
void StageMetrics::RecordQueueDequeue(const QueueRuntime& queue, const Payload& payload) noexcept {
#if FLOWPIPE_ENABLE_OTEL
  auto& state = observability::GetOtelState();
  if (!state.queue_metrics_enabled) {
    return;
  }

  const uint64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                              std::chrono::steady_clock::now().time_since_epoch())
                              .count();

  const uint64_t dwell_ns =
      payload.meta.enqueue_ts_ns > 0 ? now_ns - payload.meta.enqueue_ts_ns : 0;

  auto labels = std::initializer_list<
      std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>{
      {"queue", queue.name}};

  auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();

  dequeue_counter->Add(1, labels, ctx);

  if (state.latency_histograms && dwell_ns > 0) {
    dwell_histogram->Record(dwell_ns, labels, ctx);
  }

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

  auto labels = std::initializer_list<
      std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>{
      {"queue", queue.name}};

  auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();
  enqueue_counter->Add(1, labels, ctx);

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

  auto labels = std::initializer_list<
      std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>{
      {"stage", stage_name}};

  auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();

  process_counter->Add(1, labels, ctx);

  if (state.latency_histograms) {
    latency_histogram->Record(latency_ns, labels, ctx);
  }

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

  auto labels = std::initializer_list<
      std::pair<opentelemetry::nostd::string_view, opentelemetry::common::AttributeValue>>{
      {"stage", stage_name}};

  auto ctx = opentelemetry::context::RuntimeContext::GetCurrent();
  error_counter->Add(1, labels, ctx);

#else
  (void)stage_name;
#endif
}

}  // namespace flowpipe
