#include "flowpipe/observability/observability.h"

#include "flowpipe/observability/defaults.h"
#include "flowpipe/observability/local_logging.h"
#include "flowpipe/observability/observability_state.h"

// Plugin-safe logging
#include "flowpipe/observability/logging.h"

#if FLOWPIPE_ENABLE_OTEL
#include "flowpipe/observability/logging_runtime.h"
#include "flowpipe/observability/metrics.h"
#include "flowpipe/observability/tracing.h"
#endif

namespace flowpipe::observability {

// ------------------------------------------------------------
// InitFromProto
// ------------------------------------------------------------
void InitFromProto(const flowpipe::v1::ObservabilityConfig* cfg) {
  const bool debug_intent = cfg && cfg->debug();

  // Always initialize local logging so runtime logs reach stdout even when
  // OTEL is disabled or not configured.
  InitLocalLogging(debug_intent);

#if !FLOWPIPE_ENABLE_OTEL
  FP_LOG_DEBUG("observability: OTEL disabled at compile time");
  (void)cfg;
  return;
#else
  FP_LOG_DEBUG("observability: InitFromProto begin");

  // ----------------------------------------------------------
  // Load deployment-level defaults (policy)
  // ----------------------------------------------------------
  GlobalDefaults global = LoadFromEnv();

  FP_LOG_DEBUG("observability: loaded global defaults");

  // Start with global enablement
  bool metrics = global.metrics_enabled;
  bool tracing = global.tracing_enabled;
  bool logs = global.logs_enabled;

  // Debug intent defaults to flow-level preference
  bool debug = debug_intent;

  // ----------------------------------------------------------
  // Apply flow-level intent (if provided)
  // ----------------------------------------------------------
  if (cfg) {
    FP_LOG_DEBUG("observability: applying flow-level config");

    metrics &= cfg->metrics_enabled();
    tracing &= cfg->tracing_enabled();
    logs &= cfg->logs_enabled();
    // Honor flow-level debug intent for OTEL exporters
    debug = cfg->debug();
  } else {
    FP_LOG_DEBUG("observability: no flow-level config provided");
  }

  FP_LOG_DEBUG_FMT(
      "observability: effective enablement "
      "(tracing=%d, logs=%d, metrics=%d, debug=%d)",
      tracing, logs, metrics, debug);

  // ----------------------------------------------------------
  // Initialize signals
  //
  // Preferred order:
  //   Traces → Logs → Metrics
  // ----------------------------------------------------------
  if (tracing) {
    FP_LOG_DEBUG("observability: initializing tracing");
    InitTracing(cfg, global, debug);
  } else {
    FP_LOG_DEBUG("observability: tracing disabled");
  }

  if (logs) {
    FP_LOG_DEBUG("observability: initializing logging");
    InitLogging(cfg, global, debug);
  } else {
    FP_LOG_DEBUG("observability: logging disabled");
  }

  if (metrics) {
    FP_LOG_DEBUG("observability: initializing metrics");
    InitMetrics(cfg, global, debug);
  } else {
    FP_LOG_DEBUG("observability: metrics disabled");
  }

  FP_LOG_DEBUG("observability: InitFromProto complete");
#endif
}

// ------------------------------------------------------------
// ShutdownObservability
// ------------------------------------------------------------
void ShutdownObservability() {
#if !FLOWPIPE_ENABLE_OTEL
  return;
#else
  FP_LOG_DEBUG("observability: shutdown begin");

  auto& state = GetOtelState();

  // ----------------------------------------------------------
  // Logs (flush first)
  // ----------------------------------------------------------
  if (state.logger_provider) {
    FP_LOG_DEBUG("observability: shutting down logger provider");
    state.logger_provider->Shutdown();
    state.logger_provider.reset();
  } else {
    FP_LOG_DEBUG("observability: logger provider not initialized");
  }

  // ----------------------------------------------------------
  // Traces (flush spans)
  // ----------------------------------------------------------
  if (state.tracer_provider) {
    FP_LOG_DEBUG("observability: shutting down tracer provider");
    state.tracer_provider->Shutdown();
    state.tracer_provider.reset();
  } else {
    FP_LOG_DEBUG("observability: tracer provider not initialized");
  }

  // ----------------------------------------------------------
  // Metrics (stop periodic readers last)
  // ----------------------------------------------------------
  if (state.meter_provider) {
    FP_LOG_DEBUG("observability: shutting down meter provider");
    state.meter_provider->Shutdown();
    state.meter_provider.reset();
  } else {
    FP_LOG_DEBUG("observability: meter provider not initialized");
  }

  FP_LOG_DEBUG("observability: shutdown complete");
#endif
}

}  // namespace flowpipe::observability
