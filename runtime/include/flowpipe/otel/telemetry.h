#pragma once

#include <chrono>
#include <string>

namespace flowpipe::otel {

struct TelemetryConfig {
  std::string service_name;
  std::string endpoint;
  std::chrono::milliseconds metrics_export_interval{std::chrono::milliseconds{5000}};
};

// Initialize metrics, tracing, and logging exporters when OpenTelemetry
// support is enabled at build time. No-ops when disabled.
void Init(const TelemetryConfig& config);

// Release global providers and readers. Safe to call even if Init() was a
// no-op.
void Shutdown();

}  // namespace flowpipe::otel
