#include "flowpipe/observability/logging_runtime.h"

#include "flowpipe/observability/observability_state.h"

#if FLOWPIPE_ENABLE_OTEL

#include <chrono>
#include <memory>
#include <string>

// ---- OpenTelemetry: Logs (SDK)
#include <opentelemetry/sdk/logs/batch_log_record_processor.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_factory.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor_factory.h>

// ---- OpenTelemetry: OTLP exporters
#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_log_record_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h>

// ---- OpenTelemetry: API
#include <opentelemetry/logs/provider.h>

namespace logs_sdk = opentelemetry::sdk::logs;
namespace otlp = opentelemetry::exporter::otlp;

namespace flowpipe::observability {

// ------------------------------------------------------------
// Batch processor option mapping
// ------------------------------------------------------------
static logs_sdk::BatchLogRecordProcessorOptions CreateBatchLoggingOptions(
    const flowpipe::v1::ObservabilityConfig::LoggingConfig::BatchConfig& cfg, bool debug) {
  logs_sdk::BatchLogRecordProcessorOptions opts;

  if (cfg.max_queue_size() > 0)
    opts.max_queue_size = cfg.max_queue_size();

  if (cfg.max_export_batch_size() > 0)
    opts.max_export_batch_size = cfg.max_export_batch_size();

  if (cfg.schedule_delay_ms() > 0)
    opts.schedule_delay_millis = std::chrono::milliseconds(cfg.schedule_delay_ms());

  if (cfg.export_timeout_ms() > 0)
    opts.export_timeout_millis = std::chrono::milliseconds(cfg.export_timeout_ms());

  // Debug intent → faster flush, smaller batches
  if (debug) {
    opts.schedule_delay_millis = std::chrono::milliseconds(200);
    opts.max_export_batch_size = 64;
  }

  return opts;
}

// ------------------------------------------------------------
// Runtime-only logging initialization
// ------------------------------------------------------------
void InitLogging(const flowpipe::v1::ObservabilityConfig* cfg, const GlobalDefaults& global,
                 bool debug) {
  auto& state = GetOtelState();

  // Idempotent
  if (!cfg || state.logger_provider)
    return;

  const auto& logging_cfg = cfg->logging();

  // ----------------------------------------------------------
  // Resolve endpoint & transport
  // ----------------------------------------------------------
  std::string endpoint = global.otlp_endpoint;
  auto transport = flowpipe::v1::OTLP_TRANSPORT_GRPC;

  if (!cfg->otlp_endpoint().empty() && global.allow_endpoint_overrides)
    endpoint = cfg->otlp_endpoint();

  if (cfg->transport() != flowpipe::v1::OTLP_TRANSPORT_UNSPECIFIED)
    transport = cfg->transport();

  // ----------------------------------------------------------
  // Exporter
  // ----------------------------------------------------------
  std::unique_ptr<logs_sdk::LogRecordExporter> exporter;

  if (transport == flowpipe::v1::OTLP_TRANSPORT_HTTP) {
    otlp::OtlpHttpLogRecordExporterOptions opts;
    opts.url = endpoint;
    exporter = otlp::OtlpHttpLogRecordExporterFactory::Create(opts);
  } else {
    otlp::OtlpGrpcLogRecordExporterOptions opts;
    opts.endpoint = endpoint;
    opts.use_ssl_credentials = global.otlp_use_ssl_credentials;
    exporter = otlp::OtlpGrpcLogRecordExporterFactory::Create(opts);
  }

  // ----------------------------------------------------------
  // Processor
  // ----------------------------------------------------------
  std::unique_ptr<logs_sdk::LogRecordProcessor> processor;

  if (logging_cfg.processor() ==
      flowpipe::v1::ObservabilityConfig::LoggingConfig::LOG_PROCESSOR_SIMPLE) {
    processor = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
  } else {
    auto opts = CreateBatchLoggingOptions(logging_cfg.batch(), debug);
    processor = logs_sdk::BatchLogRecordProcessorFactory::Create(std::move(exporter), opts);
  }

  // ----------------------------------------------------------
  // Provider (SDK → API)
  // ----------------------------------------------------------
  state.logger_provider = logs_sdk::LoggerProviderFactory::Create(std::move(processor));
  std::shared_ptr<opentelemetry::logs::LoggerProvider> api_provider = state.logger_provider;
  opentelemetry::logs::Provider::SetLoggerProvider(api_provider);
}

}  // namespace flowpipe::observability

#else  // FLOWPIPE_ENABLE_OTEL

namespace flowpipe::observability {

void InitLogging(const flowpipe::v1::ObservabilityConfig*, const GlobalDefaults&, bool) {}

}  // namespace flowpipe::observability

#endif
