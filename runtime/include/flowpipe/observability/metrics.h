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
void InitMetrics(const flowpipe::v1::ObservabilityConfig* cfg, const GlobalDefaults& global,
                 bool debug);

}  // namespace flowpipe::observability
