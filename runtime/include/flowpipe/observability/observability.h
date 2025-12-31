#pragma once

#include "flowpipe/v1/observability.pb.h"

namespace flowpipe::observability {

// ------------------------------------------------------------
// InitFromProto
// ------------------------------------------------------------
// Main observability initialization entry point.
//
// - Initializes logging, tracing, and metrics based on the
//   provided ObservabilityConfig.
// - Safe to call multiple times (idempotent).
// - If OpenTelemetry is disabled at build time, this is a no-op.
//
void InitFromProto(const flowpipe::v1::ObservabilityConfig* cfg);

// ------------------------------------------------------------
// ShutdownObservability
// ------------------------------------------------------------
// Gracefully shuts down observability providers and exporters.
//
// Shutdown order:
//   1. Logs
//   2. Traces
//   3. Metrics
//
// - Flushes pending telemetry
// - Stops background threads
// - Safe to call multiple times
// - No-op if OpenTelemetry is disabled
//
void ShutdownObservability();

}  // namespace flowpipe::observability
