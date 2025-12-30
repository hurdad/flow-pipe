#include "flowpipe/otel/telemetry.h"

#include <string>

#include "metrics/otel_metrics.h"

#ifdef FLOWPIPE_ENABLE_OTEL
// API
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/trace/provider.h>

// SDK
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>

namespace otel_trace = opentelemetry::trace;
namespace otel_logs = opentelemetry::logs;
namespace otel_resource = opentelemetry::sdk::resource;
namespace otel_sdktrace = opentelemetry::sdk::trace;
namespace otel_sdklogs = opentelemetry::sdk::logs;
namespace otel_otlp = opentelemetry::exporter::otlp;

namespace {

std::shared_ptr<otel_sdktrace::TracerProvider> tracer_provider;
std::shared_ptr<otel_sdklogs::LoggerProvider> logger_provider;

otel_resource::Resource CreateResource(const std::string& service_name) {
  return otel_resource::Resource::Create({
      {"service.name", service_name},
      {"service.namespace", "flow-pipe"},
  });
}

}  // namespace

#endif  // FLOWPIPE_ENABLE_OTEL

namespace flowpipe::otel {

void Init(const TelemetryConfig& config) {
#ifdef FLOWPIPE_ENABLE_OTEL
  const auto resource = CreateResource(config.service_name);

  // ---- Traces ----
  {
    otel_otlp::OtlpGrpcExporterOptions trace_options;
    trace_options.endpoint = config.endpoint;
    trace_options.use_ssl_credentials = false;

    auto exporter = otel_otlp::OtlpGrpcExporterFactory::Create(trace_options);
    auto processor = std::make_unique<otel_sdktrace::BatchSpanProcessor>(std::move(exporter));

    tracer_provider = std::make_shared<otel_sdktrace::TracerProvider>(
        std::move(processor), resource);

    otel_trace::Provider::SetTracerProvider(tracer_provider);
  }

  // ---- Metrics ----
  metrics::Metrics::Init(config.service_name, config.endpoint);

  // ---- Logs ----
  {
    otel_otlp::OtlpGrpcLogRecordExporterOptions log_options;
    log_options.endpoint = config.endpoint;
    log_options.use_ssl_credentials = false;

    auto exporter = otel_otlp::OtlpGrpcLogRecordExporterFactory::Create(log_options);
    auto processor = std::make_unique<otel_sdklogs::BatchLogRecordProcessor>(std::move(exporter));

    logger_provider = std::make_shared<otel_sdklogs::LoggerProvider>(
        std::move(processor), resource);

    otel_logs::Provider::SetLoggerProvider(logger_provider);
  }
#else
  (void)config;
#endif  // FLOWPIPE_ENABLE_OTEL
}

void Shutdown() {
#ifdef FLOWPIPE_ENABLE_OTEL
  metrics::Metrics::Shutdown();

  logger_provider.reset();
  tracer_provider.reset();

  otel_logs::Provider::SetLoggerProvider(nullptr);
  otel_trace::Provider::SetTracerProvider(nullptr);
#endif  // FLOWPIPE_ENABLE_OTEL
}

}  // namespace flowpipe::otel
