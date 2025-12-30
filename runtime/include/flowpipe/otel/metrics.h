#pragma once

#include <cstdint>
#include <chrono>
#include <string>

namespace flowpipe::metrics {

class Metrics {
 public:
  static void Init(const std::string& service_name, const std::string& otlp_endpoint,
                   std::chrono::milliseconds export_interval);

  static void Shutdown();

  // Counters
  static void FlowStarted();
  static void FlowCompleted();
  static void StageProcessed(const std::string& stage);

  // Gauges
  static void QueueDepth(const std::string& queue, int64_t depth);
};

}  // namespace flowpipe::metrics
