#include "flowpipe/observability/logging.h"

// Local logging (spdlog)
#include <spdlog/spdlog.h>

#if FLOWPIPE_ENABLE_OTEL

// ---- OpenTelemetry: Logs (API)
#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/logger_provider.h>
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/logs/severity.h>

// ---- OpenTelemetry: Common
#include <opentelemetry/common/attribute_value.h>

#endif  // FLOWPIPE_ENABLE_OTEL

namespace flowpipe::observability {

#if FLOWPIPE_ENABLE_OTEL

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

#endif  // FLOWPIPE_ENABLE_OTEL

// ------------------------------------------------------------
// Emit log record (runtime fan-out point)
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

#if FLOWPIPE_ENABLE_OTEL
  // ----------------------------------------------------------
  // 2) OTEL logging (optional)
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
#endif  // FLOWPIPE_ENABLE_OTEL
}

}  // namespace flowpipe::observability
