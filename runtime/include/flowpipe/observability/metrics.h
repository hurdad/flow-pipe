#pragma once

#include "defaults.h"
#include "flowpipe/v1/observability.pb.h"

namespace flowpipe::observability {

// Initialize OTEL metrics from metrics config.
// This function is a no-op when OTEL is disabled.
void InitMetrics(const flowpipe::v1::ObservabilityConfig::MetricsConfig* cfg,
                 const GlobalDefaults& global, bool debug);

}  // namespace flowpipe::observability
