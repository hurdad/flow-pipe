#include "flowpipe/observability/tracing.h"

#if FLOWPIPE_ENABLE_OTEL

#include <opentelemetry/exporters/otlp/otlp_grpc_exporter.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/provider.h>

namespace trace_sdk = opentelemetry::sdk::trace;
namespace otlp = opentelemetry::exporter::otlp;

namespace flowpipe::observability {

void InitTracing(const flowpipe::v1::ObservabilityConfig::TracingConfig* cfg,
                 const GlobalDefaults& global, bool /*debug*/) {
  // Start with global endpoint
  std::string endpoint = global.tracing_endpoint;

  // Default to gRPC transport
  auto transport = flowpipe::v1::OTLP_TRANSPORT_GRPC;

  // Apply flow overrides if allowed
  if (cfg) {
    if (!cfg->otlp_endpoint().empty() && global.allow_endpoint_overrides) {
      endpoint = cfg->otlp_endpoint();
    }
    transport = cfg->transport();
  }

  // Create exporter based on transport
  std::unique_ptr<trace_sdk::SpanExporter> exporter;

  if (transport == flowpipe::v1::OTLP_TRANSPORT_HTTP) {
    otlp::OtlpHttpExporterOptions opts;
    opts.url = endpoint;
 //   exporter = std::make_unique<otlp::OtlpHttpExporter>(opts);
  } else {
    otlp::OtlpGrpcExporterOptions opts;
    opts.endpoint = endpoint;
  //  exporter = std::make_unique<otlp::OtlpGrpcExporter>(opts);
  }

  // Use batch span processor
//  auto processor = std::make_unique<trace_sdk::BatchSpanProcessor>(std::move(exporter));

  // Install tracer provider
//  auto provider = std::make_shared<trace_sdk::TracerProvider>(std::move(processor));

  //opentelemetry::trace::Provider::SetTracerProvider(provider);
}

}  // namespace flowpipe::observability

#else

// No-op implementation when OTEL is disabled
namespace flowpipe::observability {
void InitTracing(const flowpipe::v1::ObservabilityConfig::TracingConfig*, const GlobalDefaults&,
                 bool) {}
}  // namespace flowpipe::observability

#endif
