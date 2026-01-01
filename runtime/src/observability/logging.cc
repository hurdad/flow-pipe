#include "flowpipe/observability/logging.h"

#include <spdlog/spdlog.h>

#include "flowpipe/observability/local_logging.h"
#include "flowpipe/observability/observability_state.h"

#if FLOWPIPE_ENABLE_OTEL

#include <chrono>
#include <memory>
#include <string>

// ---- OpenTelemetry: Logs (API)
#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/logger_provider.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/logs/severity.h>

// ---- OpenTelemetry: Common
#include <opentelemetry/common/attribute_value.h>

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

namespace logs_sdk = opentelemetry::sdk::logs;
namespace otlp = opentelemetry::exporter::otlp;

namespace flowpipe::observability {

// ------------------------------------------------------------
// Severity mapping (runtime → OTEL)
// ------------------------------------------------------------
static opentelemetry::logs::Severity ToOtelSeverity(LogLevel level) {
  using S = opentelemetry::logs::Severity;
  switch (level) {
    case LogLevel::Debug:
      return S::kDebug;
    case LogLevel::Info:
      return S::kInfo;
    case LogLevel::Warn:
      return S::kWarn;
    case LogLevel::Error:
      return S::kError;
    case LogLevel::Fatal:
      return S::kFatal;
  }
  return S::kInfo;
}

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

  // Debug intent → faster flush
  if (debug) {
    opts.schedule_delay_millis = std::chrono::milliseconds(200);
    opts.max_export_batch_size = 64;
  }

  return opts;
}

// ------------------------------------------------------------
// Init logging (SDK setup only)
// ------------------------------------------------------------
void InitLogging(const flowpipe::v1::ObservabilityConfig::LoggingConfig* cfg,
                 const GlobalDefaults& global, bool debug) {
  auto& state = GetOtelState();

  if (!cfg || state.logger_provider)
    return;

  std::string endpoint = global.logging_endpoint;
  auto transport = flowpipe::v1::OTLP_TRANSPORT_GRPC;

  if (!cfg->otlp_endpoint().empty() && global.allow_endpoint_overrides)
    endpoint = cfg->otlp_endpoint();

  if (cfg->transport() != flowpipe::v1::OTLP_TRANSPORT_UNSPECIFIED)
    transport = cfg->transport();

  std::unique_ptr<logs_sdk::LogRecordExporter> exporter;

  if (transport == flowpipe::v1::OTLP_TRANSPORT_HTTP) {
    otlp::OtlpHttpLogRecordExporterOptions opts;
    opts.url = endpoint;
    exporter = otlp::OtlpHttpLogRecordExporterFactory::Create(opts);
  } else {
    otlp::OtlpGrpcLogRecordExporterOptions opts;
    opts.endpoint = endpoint;
    exporter = otlp::OtlpGrpcLogRecordExporterFactory::Create(opts);
  }

  std::unique_ptr<logs_sdk::LogRecordProcessor> processor;

  if (cfg->processor() == flowpipe::v1::ObservabilityConfig::LoggingConfig::LOG_PROCESSOR_SIMPLE) {
    processor = logs_sdk::SimpleLogRecordProcessorFactory::Create(std::move(exporter));
  } else {
    auto opts = CreateBatchLoggingOptions(cfg->batch(), debug);
    processor = logs_sdk::BatchLogRecordProcessorFactory::Create(std::move(exporter), opts);
  }

  state.logger_provider = logs_sdk::LoggerProviderFactory::Create(std::move(processor));
  std::shared_ptr<opentelemetry::logs::LoggerProvider> api_provider = state.logger_provider;
  opentelemetry::logs::Provider::SetLoggerProvider(api_provider);
}

// ------------------------------------------------------------
// Emit log record (dual logging: spdlog + OTEL)
// ------------------------------------------------------------
void Log(LogLevel level, const std::string& message, const char* file, int line) {
  // ----------------------------------------------------------
  // 1) Local logging (always on)
  // ----------------------------------------------------------
  switch (level) {
    case LogLevel::Debug:
      spdlog::debug(message);
      break;
    case LogLevel::Info:
      spdlog::info(message);
      break;
    case LogLevel::Warn:
      spdlog::warn(message);
      break;
    case LogLevel::Error:
      spdlog::error(message);
      break;
    case LogLevel::Fatal:
      spdlog::critical(message);
      break;
  }

  // ----------------------------------------------------------
  // 2) OTEL logging (guarded)
  // ----------------------------------------------------------
  auto provider = opentelemetry::logs::Provider::GetLoggerProvider();
  if (!provider)
    return;

  auto logger = provider->GetLogger("flowpipe.runtime");
  if (!logger)
    return;

  auto record = logger->CreateLogRecord();
  if (!record)
    return;

  using opentelemetry::common::AttributeValue;

  if (file && line > 0) {
    logger->EmitLogRecord(std::move(record), ToOtelSeverity(level), message,
                          std::initializer_list<std::pair<std::string, AttributeValue>>{
                              {"code.filepath", AttributeValue{file}},
                              {"code.lineno", AttributeValue{line}},
                          });
  } else if (file) {
    logger->EmitLogRecord(std::move(record), ToOtelSeverity(level), message,
                          std::initializer_list<std::pair<std::string, AttributeValue>>{
                              {"code.filepath", AttributeValue{file}},
                          });
  } else if (line > 0) {
    logger->EmitLogRecord(std::move(record), ToOtelSeverity(level), message,
                          std::initializer_list<std::pair<std::string, AttributeValue>>{
                              {"code.lineno", AttributeValue{line}},
                          });
  } else {
    logger->EmitLogRecord(std::move(record), ToOtelSeverity(level), message);
  }
}

}  // namespace flowpipe::observability

#else  // FLOWPIPE_ENABLE_OTEL

#include <spdlog/spdlog.h>

namespace flowpipe::observability {

void InitLogging(const flowpipe::v1::ObservabilityConfig::LoggingConfig*, const GlobalDefaults&,
                 bool) {}

void Log(LogLevel level, const std::string& message, const char*, int) {
  // Local logging still works even without OTEL
  switch (level) {
    case LogLevel::Debug:
      spdlog::debug(message);
      break;
    case LogLevel::Info:
      spdlog::info(message);
      break;
    case LogLevel::Warn:
      spdlog::warn(message);
      break;
    case LogLevel::Error:
      spdlog::error(message);
      break;
    case LogLevel::Fatal:
      spdlog::critical(message);
      break;
  }
}

}  // namespace flowpipe::observability

#endif
