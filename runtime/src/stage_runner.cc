#include "flowpipe/stage_runner.h"

#include <chrono>
#include <cstring>

#if FLOWPIPE_ENABLE_OTEL
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span_context.h>
#include <opentelemetry/trace/span_id.h>
#include <opentelemetry/trace/trace_id.h>

#include "flowpipe/observability/observability_state.h"
#endif

namespace flowpipe {

// ------------------------------------------------------------
// Time helper (monotonic, nanoseconds)
// ------------------------------------------------------------
static inline uint64_t now_ns() noexcept {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

#if FLOWPIPE_ENABLE_OTEL

static inline bool StageSpansEnabled() noexcept {
  return flowpipe::observability::GetOtelState().stage_spans_enabled;
}

static opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer() {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer("flowpipe.runtime", "1.0.0");
}

// Build a parent span context from PayloadMeta (if present)
static opentelemetry::trace::SpanContext SpanContextFromPayload(const PayloadMeta& meta) {
  if (!meta.has_trace()) {
    return opentelemetry::trace::SpanContext::GetInvalid();
  }

  opentelemetry::trace::TraceId trace_id{
      opentelemetry::nostd::span<const uint8_t, PayloadMeta::trace_id_size>(meta.trace_id)};

  opentelemetry::trace::SpanId span_id{
      opentelemetry::nostd::span<const uint8_t, PayloadMeta::span_id_size>(meta.span_id)};

  opentelemetry::trace::TraceFlags flags{static_cast<uint8_t>(meta.flags & 0xFF)};

  return opentelemetry::trace::SpanContext{trace_id, span_id, flags, /*is_remote=*/true};
}

// Write child span context back into payload metadata
static inline void WriteSpanToPayload(const opentelemetry::trace::SpanContext& ctx,
                                      PayloadMeta& meta) noexcept {
  if (!ctx.IsValid()) {
    std::memset(meta.trace_id, 0, PayloadMeta::trace_id_size);
    std::memset(meta.span_id, 0, PayloadMeta::span_id_size);
    meta.flags = 0;
    return;
  }

  ctx.trace_id().CopyBytesTo(
      opentelemetry::nostd::span<uint8_t, PayloadMeta::trace_id_size>(meta.trace_id));

  ctx.span_id().CopyBytesTo(
      opentelemetry::nostd::span<uint8_t, PayloadMeta::span_id_size>(meta.span_id));

  meta.flags = ctx.trace_flags().flags();
}

#endif  // FLOWPIPE_ENABLE_OTEL

// ------------------------------------------------------------
// Source stage runner
// ------------------------------------------------------------
void RunSourceStage(ISourceStage* stage, StageContext& ctx, QueueRuntime& output,
                    StageMetrics* metrics) {
  while (!ctx.stop.stop_requested()) {
    Payload payload;

#if FLOWPIPE_ENABLE_OTEL
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
    std::unique_ptr<opentelemetry::trace::Scope> scope;

    if (StageSpansEnabled()) {
      auto tracer = GetTracer();
      span = tracer->StartSpan(stage->name());
      scope = std::make_unique<opentelemetry::trace::Scope>(tracer->WithActiveSpan(span));
    }
#endif

    const uint64_t start_ns = now_ns();
    const bool produced = stage->produce(ctx, payload);
    const uint64_t end_ns = now_ns();

#if FLOWPIPE_ENABLE_OTEL
    if (span) {
      WriteSpanToPayload(span->GetContext(), payload.meta);
      span->End();
    }
#endif

    if (!produced) {
      break;
    }

    if (metrics) {
      metrics->RecordStageLatency(stage->name().c_str(), end_ns - start_ns);
    }

    payload.meta.enqueue_ts_ns = now_ns();
    if (!output.queue->push(std::move(payload), ctx.stop)) {
      break;
    }

    if (metrics) {
      metrics->RecordQueueEnqueue(output);
    }
  }

  output.queue->close();
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

    const Payload& in_payload = *item;

    if (metrics) {
      metrics->RecordQueueDequeue(input, in_payload);
    }

#if FLOWPIPE_ENABLE_OTEL
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
    std::unique_ptr<opentelemetry::trace::Scope> scope;

    if (StageSpansEnabled()) {
      auto tracer = GetTracer();
      auto parent_ctx = SpanContextFromPayload(in_payload.meta);

      opentelemetry::trace::StartSpanOptions opts;
      if (parent_ctx.IsValid()) {
        opts.parent = parent_ctx;
      }

      span = tracer->StartSpan(stage->name(), opts);
      scope = std::make_unique<opentelemetry::trace::Scope>(tracer->WithActiveSpan(span));
    }
#endif

    Payload out_payload;

    const uint64_t start_ns = now_ns();
    stage->process(ctx, in_payload, out_payload);
    const uint64_t end_ns = now_ns();

#if FLOWPIPE_ENABLE_OTEL
    if (span) {
      WriteSpanToPayload(span->GetContext(), out_payload.meta);
      span->End();
    }
#endif

    if (metrics) {
      metrics->RecordStageLatency(stage->name().c_str(), end_ns - start_ns);
    }

    out_payload.meta.enqueue_ts_ns = now_ns();
    if (!output.queue->push(std::move(out_payload), ctx.stop)) {
      break;
    }

    if (metrics) {
      metrics->RecordQueueEnqueue(output);
    }
  }

  output.queue->close();
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

    if (metrics) {
      metrics->RecordQueueDequeue(input, payload);
    }

#if FLOWPIPE_ENABLE_OTEL
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
    std::unique_ptr<opentelemetry::trace::Scope> scope;

    if (StageSpansEnabled()) {
      auto tracer = GetTracer();
      auto parent_ctx = SpanContextFromPayload(payload.meta);

      opentelemetry::trace::StartSpanOptions opts;
      if (parent_ctx.IsValid()) {
        opts.parent = parent_ctx;
      }

      span = tracer->StartSpan(stage->name(), opts);
      scope = std::make_unique<opentelemetry::trace::Scope>(tracer->WithActiveSpan(span));
    }
#endif

    const uint64_t start_ns = now_ns();
    stage->consume(ctx, payload);
    const uint64_t end_ns = now_ns();

#if FLOWPIPE_ENABLE_OTEL
    if (span) {
      span->End();
    }
#endif

    if (metrics) {
      metrics->RecordStageLatency(stage->name().c_str(), end_ns - start_ns);
    }
  }
}

}  // namespace flowpipe
