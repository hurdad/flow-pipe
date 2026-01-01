#pragma once

#include <fmt/core.h>

#include <cstdint>
#include <string>

#include "defaults.h"
#include "flowpipe/v1/observability.pb.h"

namespace flowpipe::observability {

// ------------------------------------------------------------
// Logging severity (runtime-level, OTEL-agnostic)
// ------------------------------------------------------------
enum class LogLevel : uint8_t {
  Debug = 0,
  Info = 1,
  Warn = 2,
  Error = 3,
  Fatal = 4,
};

// ------------------------------------------------------------
// Initialize logging subsystem
// ------------------------------------------------------------
void InitLogging(const flowpipe::v1::ObservabilityConfig::LoggingConfig* cfg,
                 const GlobalDefaults& global, bool debug);

// ------------------------------------------------------------
// Emit a log record (internal; prefer macros)
// ------------------------------------------------------------
void Log(LogLevel level, const std::string& message, const char* file = nullptr, int line = 0);

// ------------------------------------------------------------
// Lazy formatting helper (single choke point)
// ------------------------------------------------------------
template <typename... Args>
inline void LogFmt(LogLevel level, const char* file, int line, fmt::format_string<Args...> fmt_str,
                   Args&&... args) {
  // NOTE:
  // Later you can add runtime level checks here
  // without changing any call sites.
  Log(level, fmt::format(fmt_str, std::forward<Args>(args)...), file, line);
}

}  // namespace flowpipe::observability

// ============================================================
// Logging macros (preferred API)
// ============================================================

// ---- plain string
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

// ---- lazy formatting (hot-path safe)
#define FP_LOG_DEBUG_FMT(fmt, ...)                                                        \
  ::flowpipe::observability::LogFmt(::flowpipe::observability::LogLevel::Debug, __FILE__, \
                                    __LINE__, fmt, ##__VA_ARGS__)

#define FP_LOG_INFO_FMT(fmt, ...)                                                                  \
  ::flowpipe::observability::LogFmt(::flowpipe::observability::LogLevel::Info, __FILE__, __LINE__, \
                                    fmt, ##__VA_ARGS__)

#define FP_LOG_WARN_FMT(fmt, ...)                                                                  \
  ::flowpipe::observability::LogFmt(::flowpipe::observability::LogLevel::Warn, __FILE__, __LINE__, \
                                    fmt, ##__VA_ARGS__)

#define FP_LOG_ERROR_FMT(fmt, ...)                                                        \
  ::flowpipe::observability::LogFmt(::flowpipe::observability::LogLevel::Error, __FILE__, \
                                    __LINE__, fmt, ##__VA_ARGS__)

#define FP_LOG_FATAL_FMT(fmt, ...)                                                        \
  ::flowpipe::observability::LogFmt(::flowpipe::observability::LogLevel::Fatal, __FILE__, \
                                    __LINE__, fmt, ##__VA_ARGS__)
