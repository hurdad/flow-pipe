#include "flowpipe/observability/local_logging.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace flowpipe::observability {

void InitLocalLogging(bool debug) {
  // Create a colorized stdout sink
  auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

  // Create logger
  auto logger = std::make_shared<spdlog::logger>("flowpipe", sink);

  // Timestamp + level + message
  logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  // Set verbosity
  logger->set_level(debug ? spdlog::level::debug : spdlog::level::info);

  // Make this the default logger
  spdlog::set_default_logger(logger);

  // Never throw from logging
  spdlog::set_error_handler([](const std::string&) {});
}

}  // namespace flowpipe::observability
