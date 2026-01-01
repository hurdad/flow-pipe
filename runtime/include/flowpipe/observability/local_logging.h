#pragma once

namespace flowpipe::observability {

// ------------------------------------------------------------
// Initialize local (stdout/stderr) logging
//
// - Uses spdlog
// - Always safe to call
// - Should be called BEFORE InitLogging (OTEL)
//
// Params:
//   debug: enables debug-level logging
// ------------------------------------------------------------
void InitLocalLogger(bool debug);

}  // namespace flowpipe::observability
