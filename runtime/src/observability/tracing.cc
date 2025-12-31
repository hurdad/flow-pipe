#include "flowpipe/observability/tracing.h"

#include "flowpipe/observability/observability_state.h"

#if FLOWPIPE_ENABLE_OTEL

#include <chrono>
#include <memory>
#include <string>

// ---- OpenTelemetry: Tracing (SDK)
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>

// ---- OpenTelemetry: OTLP exporters
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>

namespace trace_sdk = opentelemetry::sdk::trace;
namespace otlp = opentelemetry::exporter::otlp;

namespace flowpipe::observability {

// ------------------------------------------------------------
// Batch processor option mapping
// ------------------------------------------------------------
static trace_sdk::BatchSpanProcessorOptions CreateBatchTraceOptions(
    const flowpipe::v1::ObservabilityConfig::TracingConfig::BatchConfig& cfg, bool debug) {
  trace_sdk::BatchSpanProcessorOptions opts;

  if (cfg.max_queue_size() > 0)
    opts.max_queue_size = static_cast<size_t>(cfg.max_queue_size());

  if (cfg.max_export_batch_size() > 0)
    opts.max_export_batch_size = static_cast<size_t>(cfg.max_export_batch_size());

  if (cfg.schedule_delay_ms() > 0)
    opts.schedule_delay_millis = std::chrono::milliseconds(cfg.schedule_delay_ms());

  if (cfg.export_timeout_ms() > 0)
    opts.export_timeout = std::chrono::milliseconds(cfg.export_timeout_ms());

  // Debug intent → faster visibility
  if (debug) {
    opts.schedule_delay_millis = std::chrono::milliseconds(200);
    opts.max_export_batch_size = 64;
  }

  // Safety clamp
  if (opts.max_queue_size > 0 && opts.max_export_batch_size > opts.max_queue_size) {
    opts.max_export_batch_size = opts.max_queue_size;
  }

  return opts;
}

// ------------------------------------------------------------
// Init tracing
// ------------------------------------------------------------
void InitTracing(const flowpipe::v1::ObservabilityConfig::TracingConfig* cfg,
                 const GlobalDefaults& global, bool debug) {
  auto& state = GetOtelState();

  if (!cfg || state.tracer_provider)
    return;

  // ----------------------------------------------------------
  // Endpoint & transport resolution
  // ----------------------------------------------------------
  std::string endpoint = global.tracing_endpoint;
  auto transport = flowpipe::v1::OTLP_TRANSPORT_GRPC;

  if (!cfg->otlp_endpoint().empty() && global.allow_endpoint_overrides)
    endpoint = cfg->otlp_endpoint();

  if (cfg->transport() != flowpipe::v1::OTLP_TRANSPORT_UNSPECIFIED)
    transport = cfg->transport();

  // ----------------------------------------------------------
  // Exporter
  // ----------------------------------------------------------
  std::unique_ptr<trace_sdk::SpanExporter> exporter;

  if (transport == flowpipe::v1::OTLP_TRANSPORT_HTTP) {
    otlp::OtlpHttpExporterOptions opts;
    opts.url = endpoint;
    exporter = otlp::OtlpHttpExporterFactory::Create(opts);
  } else {
    otlp::OtlpGrpcExporterOptions opts;
    opts.endpoint = endpoint;
    exporter = otlp::OtlpGrpcExporterFactory::Create(opts);
  }

  // ----------------------------------------------------------
  // Processor
  // ----------------------------------------------------------
  std::unique_ptr<trace_sdk::SpanProcessor> processor;

  if (cfg->processor() ==
      flowpipe::v1::ObservabilityConfig::TracingConfig::TRACE_PROCESSOR_SIMPLE) {
    processor = trace_sdk::SimpleSpanProcessorFactory::Create(std::move(exporter));
  } else {
    auto opts = CreateBatchTraceOptions(cfg->batch(), debug);
    processor = trace_sdk::BatchSpanProcessorFactory::Create(std::move(exporter), opts);
  }

  // ----------------------------------------------------------
  // Provider (SDK → API)
  // ----------------------------------------------------------
  state.tracer_provider = trace_sdk::TracerProviderFactory::Create(std::move(processor));
  std::shared_ptr<opentelemetry::trace::TracerProvider> api_provider = state.tracer_provider;
  opentelemetry::trace::Provider::SetTracerProvider(api_provider);
}

}  // namespace flowpipe::observability

#else  // FLOWPIPE_ENABLE_OTEL

namespace flowpipe::observability {

void InitTracing(const flowpipe::v1::ObservabilityConfig::TracingConfig*, const GlobalDefaults&,
                 bool) {}

}  // namespace flowpipe::observability

#endif
