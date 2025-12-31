#pragma once

#include <string>

namespace flowpipe::observability {

// ------------------------------------------------------------
// GlobalDefaults
// ------------------------------------------------------------
// Deployment-level observability policy loaded from env vars.
// This defines the *maximum capability* allowed at runtime.
// Flow configs can only narrow, never expand this.
//
struct GlobalDefaults {
  bool metrics_enabled = true;   // Are metrics allowed at all?
  bool tracing_enabled = false;  // Are traces allowed at all?
  bool logs_enabled = false;     // Are logs allowed at all?

  // Default OTLP endpoints (used when flow does not override)
  std::string metrics_endpoint;  // Metrics OTLP endpoint
  std::string tracing_endpoint;  // Tracing OTLP endpoint
  std::string logging_endpoint;  // Logging OTLP endpoint

  // Whether flow-level endpoint overrides are permitted
  bool allow_endpoint_overrides = false;
};

// Load GlobalDefaults from environment variables.
// This function is the *only* place env vars are read.
GlobalDefaults LoadFromEnv();

}  // namespace flowpipe::observability
