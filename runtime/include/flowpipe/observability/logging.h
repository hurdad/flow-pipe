#pragma once

#include <cstdint>
#include <string>

namespace flowpipe::observability {

// ------------------------------------------------------------
// Logging severity (public API, runtime-agnostic)
// ------------------------------------------------------------
enum class LogLevel : uint8_t {
  Debug = 0,
  Info = 1,
  Warn = 2,
  Error = 3,
  Fatal = 4,
};

// ------------------------------------------------------------
// Emit a log record
//
// Implemented by the runtime.
// Safe to call from plugins and stages.
// ------------------------------------------------------------
void Log(LogLevel level, const std::string& message, const char* file = nullptr, int line = 0);

}  // namespace flowpipe::observability

// ============================================================
// Logging macros (preferred API)
// ============================================================
//
// These macros are:
//  - plugin-safe
//  - source-location aware
//  - OTEL-agnostic
//  - spdlog-agnostic
//

// ---- plain string logging

#define FP_LOG_DEBUG(msg)                                                                     \
  ::flowpipe::observability::Log(::flowpipe::observability::LogLevel::Debug, (msg), __FILE__, \
                                 __LINE__)

#define FP_LOG_INFO(msg)                                                                     \
  ::flowpipe::observability::Log(::flowpipe::observability::LogLevel::Info, (msg), __FILE__, \
                                 __LINE__)

#define FP_LOG_WARN(msg)                                                                     \
  ::flowpipe::observability::Log(::flowpipe::observability::LogLevel::Warn, (msg), __FILE__, \
                                 __LINE__)

#define FP_LOG_ERROR(msg)                                                                     \
  ::flowpipe::observability::Log(::flowpipe::observability::LogLevel::Error, (msg), __FILE__, \
                                 __LINE__)

#define FP_LOG_FATAL(msg)                                                                     \
  ::flowpipe::observability::Log(::flowpipe::observability::LogLevel::Fatal, (msg), __FILE__, \
                                 __LINE__)
