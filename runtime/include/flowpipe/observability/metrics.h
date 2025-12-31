#pragma once

#include "defaults.h"
#include "flowpipe/v1/observability.pb.h"

namespace flowpipe::observability {

// ------------------------------------------------------------
// Initialization
// ------------------------------------------------------------

// Initialize OTEL metrics from metrics config.
// This function is a no-op when OTEL is disabled or metrics
// are not enabled by configuration.
void InitMetrics(const flowpipe::v1::ObservabilityConfig::MetricsConfig* cfg,
                 const GlobalDefaults& global, bool debug);

// ------------------------------------------------------------
// Runtime capability helpers
// ------------------------------------------------------------

// // True if any metrics pipeline is enabled.
// bool MetricsEnabled();
//
// // Granularity controls
// bool StageMetricsEnabled();
// bool QueueMetricsEnabled();
// bool FlowMetricsEnabled();
//
// // Cost / cardinality controls
// bool LatencyHistogramsEnabled();
// bool MetricsCountersOnly();

}  // namespace flowpipe::observability
