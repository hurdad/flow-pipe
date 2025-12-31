#pragma once

#include "defaults.h"
#include "flowpipe/v1/observability.pb.h"

namespace flowpipe::observability {

// Initialize OTEL logging from logging config.
// This function is a no-op when OTEL is disabled.
void InitLogging(const flowpipe::v1::ObservabilityConfig::LoggingConfig* cfg,
                 const GlobalDefaults& global, bool debug);

}  // namespace flowpipe::observability
