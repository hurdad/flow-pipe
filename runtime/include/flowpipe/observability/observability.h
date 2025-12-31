#pragma once

#include "flowpipe/v1/observability.pb.h"

namespace flowpipe::observability {

// ------------------------------------------------------------
// InitFromProto
// ------------------------------------------------------------
// Main observability initialization entry point.
// Safe to call regardless of whether OTEL is enabled.
//
// If OTEL is disabled at build time, this becomes a no-op.
//
void InitFromProto(const flowpipe::v1::ObservabilityConfig* cfg);

}  // namespace flowpipe::observability
