#pragma once

#include "defaults.h"
#include "flowpipe/v1/observability.pb.h"

namespace flowpipe::observability {

// Initialize OTEL tracing from tracing config.
// This function is a no-op when OTEL is disabled.
void InitTracing(const flowpipe::v1::ObservabilityConfig* cfg, const GlobalDefaults& global,
                 bool debug);

}  // namespace flowpipe::observability
