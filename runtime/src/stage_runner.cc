#include "flowpipe/stage_runner.h"

#include <chrono>
#include <cstring>

#include "flowpipe/observability/logging_runtime.h"

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

static inline bool ValidateInputSchema(const QueueRuntime& queue, const Payload& payload,
                                       const char* stage_name) {
  if (queue.schema_id.empty()) {
    return true;
  }

  if (payload.meta.schema_id.empty()) {
    FP_LOG_ERROR_FMT("stage '{}' received payload without schema_id on queue '{}'", stage_name,
                     queue.name);
    return false;
  }

  if (payload.meta.schema_id != queue.schema_id) {
    FP_LOG_ERROR_FMT(
        "stage '{}' received payload with schema_id '{}' on queue '{}' (expected '{}')",
        stage_name, payload.meta.schema_id, queue.name, queue.schema_id);
    return false;
  }

  return true;
}

static inline bool ApplyOutputSchema(const QueueRuntime& queue, Payload& payload,
                                     const char* stage_name) {
  if (queue.schema_id.empty()) {
    return true;
  }

  if (payload.meta.schema_id.empty()) {
    payload.meta.schema_id = queue.schema_id;
    return true;
  }

  if (payload.meta.schema_id != queue.schema_id) {
    FP_LOG_ERROR_FMT("stage '{}' produced payload with schema_id '{}' for queue '{}' (expected "
                     "'{}')",
                     stage_name, payload.meta.schema_id, queue.name, queue.schema_id);
    return false;
  }

  return true;
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
  FP_LOG_DEBUG_FMT("source stage '{}' runner started", stage->name());

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
    bool produced = false;
    try {
      produced = stage->produce(ctx, payload);
    } catch (const std::exception& ex) {
      FP_LOG_ERROR_FMT("source stage '{}' threw exception: {}", stage->name(), ex.what());
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
#if FLOWPIPE_ENABLE_OTEL
      if (span) {
        span->End();
      }
#endif
      output.queue->close();
      break;
    } catch (...) {
      FP_LOG_ERROR_FMT("source stage '{}' threw unknown exception", stage->name());
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
#if FLOWPIPE_ENABLE_OTEL
      if (span) {
        span->End();
      }
#endif
      output.queue->close();
      break;
    }
    const uint64_t end_ns = now_ns();

#if FLOWPIPE_ENABLE_OTEL
    if (span) {
      WriteSpanToPayload(span->GetContext(), payload.meta);
      span->End();
    }
#endif

    if (!produced) {
      FP_LOG_DEBUG_FMT("source stage '{}' returned no payload (terminating)", stage->name());
      break;
    }

    if (metrics) {
      metrics->RecordStageLatency(stage->name().c_str(), end_ns - start_ns);
    }

    if (!ApplyOutputSchema(output, payload, stage->name().c_str())) {
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
      continue;
    }

    payload.meta.enqueue_ts_ns = now_ns();
    if (!output.queue->push(std::move(payload), ctx.stop)) {
      FP_LOG_DEBUG_FMT("source stage '{}' output queue closed or stop requested", stage->name());
      break;
    }

    if (metrics) {
      metrics->RecordQueueEnqueue(output);
    }
  }

  FP_LOG_DEBUG_FMT("source stage '{}' closing output queue", stage->name());

  output.queue->close();
}

// ------------------------------------------------------------
// Transform stage runner
// ------------------------------------------------------------
void RunTransformStage(ITransformStage* stage, StageContext& ctx, QueueRuntime& input,
                       QueueRuntime& output, StageMetrics* metrics) {
  FP_LOG_DEBUG_FMT("transform stage '{}' runner started", stage->name());

  while (!ctx.stop.stop_requested()) {
    auto item = input.queue->pop(ctx.stop);
    if (!item.has_value()) {
      FP_LOG_DEBUG_FMT("transform stage '{}' input queue closed", stage->name());
      break;
    }

    const Payload& in_payload = *item;

    if (metrics) {
      metrics->RecordQueueDequeue(input, in_payload);
    }

    if (!ValidateInputSchema(input, in_payload, stage->name().c_str())) {
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
      continue;
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
    try {
      stage->process(ctx, in_payload, out_payload);
    } catch (const std::exception& ex) {
      FP_LOG_ERROR_FMT("transform stage '{}' threw exception: {}", stage->name(), ex.what());
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
#if FLOWPIPE_ENABLE_OTEL
      if (span) {
        span->End();
      }
#endif
      output.queue->close();
      break;
    } catch (...) {
      FP_LOG_ERROR_FMT("transform stage '{}' threw unknown exception", stage->name());
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
#if FLOWPIPE_ENABLE_OTEL
      if (span) {
        span->End();
      }
#endif
      output.queue->close();
      break;
    }
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

    if (!ApplyOutputSchema(output, out_payload, stage->name().c_str())) {
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
      continue;
    }

    out_payload.meta.enqueue_ts_ns = now_ns();
    if (!output.queue->push(std::move(out_payload), ctx.stop)) {
      FP_LOG_DEBUG_FMT("transform stage '{}' output queue closed or stop requested", stage->name());
      break;
    }

    if (metrics) {
      metrics->RecordQueueEnqueue(output);
    }
  }

  FP_LOG_DEBUG_FMT("transform stage '{}' closing output queue", stage->name());

  output.queue->close();
}

// ------------------------------------------------------------
// Sink stage runner
// ------------------------------------------------------------
void RunSinkStage(ISinkStage* stage, StageContext& ctx, QueueRuntime& input,
                  StageMetrics* metrics) {
  FP_LOG_DEBUG_FMT("sink stage '{}' runner started", stage->name());

  while (!ctx.stop.stop_requested()) {
    auto item = input.queue->pop(ctx.stop);
    if (!item.has_value()) {
      FP_LOG_DEBUG_FMT("sink stage '{}' input queue closed", stage->name());
      break;
    }

    const Payload& payload = *item;

    if (metrics) {
      metrics->RecordQueueDequeue(input, payload);
    }

    if (!ValidateInputSchema(input, payload, stage->name().c_str())) {
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
      continue;
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
    try {
      stage->consume(ctx, payload);
    } catch (const std::exception& ex) {
      FP_LOG_ERROR_FMT("sink stage '{}' threw exception: {}", stage->name(), ex.what());
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
#if FLOWPIPE_ENABLE_OTEL
      if (span) {
        span->End();
      }
#endif
      break;
    } catch (...) {
      FP_LOG_ERROR_FMT("sink stage '{}' threw unknown exception", stage->name());
      if (metrics) {
        metrics->RecordStageError(stage->name().c_str());
      }
#if FLOWPIPE_ENABLE_OTEL
      if (span) {
        span->End();
      }
#endif
      break;
    }
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

  FP_LOG_DEBUG_FMT("sink stage '{}' runner exiting", stage->name());
}

}  // namespace flowpipe
