#pragma once

#include <fmt/core.h>

#include <utility>

#include "defaults.h"
#include "flowpipe/observability/logging.h"
#include "flowpipe/v1/observability.pb.h"

namespace flowpipe::observability {

// ------------------------------------------------------------
// Runtime-only logging initialization
// ------------------------------------------------------------
void InitLogging(const flowpipe::v1::ObservabilityConfig* cfg, const GlobalDefaults& global,
                 bool debug);

// ------------------------------------------------------------
// Runtime-only lazy formatting helper
//
// Owns formatting cost and policy.
// ------------------------------------------------------------
template <typename... Args>
inline void LogFmt(LogLevel level, const char* file, int line, fmt::format_string<Args...> fmt_str,
                   Args&&... args) {
  // Optional future hook:
  // if (!IsLogEnabled(level)) return;

  Log(level, fmt::format(fmt_str, std::forward<Args>(args)...), file, line);
}

}  // namespace flowpipe::observability

// ============================================================
// Runtime-only fmt logging macros
// ============================================================

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
