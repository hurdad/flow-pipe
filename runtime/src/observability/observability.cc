#include "flowpipe/observability/observability.h"

#include "flowpipe/observability/defaults.h"
#include "flowpipe/observability/observability_state.h"

#if FLOWPIPE_ENABLE_OTEL
#include "flowpipe/observability/logging.h"
#include "flowpipe/observability/metrics.h"
#include "flowpipe/observability/tracing.h"
#endif

namespace flowpipe::observability {

// ------------------------------------------------------------
// InitFromProto
// ------------------------------------------------------------
void InitFromProto(const flowpipe::v1::ObservabilityConfig* cfg) {
#if !FLOWPIPE_ENABLE_OTEL
  // OTEL compiled out: do nothing
  (void)cfg;
  return;
#else
  // ----------------------------------------------------------
  // Load deployment-level defaults (policy)
  // ----------------------------------------------------------
  GlobalDefaults global = LoadFromEnv();

  // Start with global enablement
  bool metrics = global.metrics_enabled;
  bool tracing = global.tracing_enabled;
  bool logs = global.logs_enabled;

  // Debug intent defaults to false
  bool debug = false;

  // ----------------------------------------------------------
  // Apply flow-level intent (if provided)
  // ----------------------------------------------------------
  if (cfg) {
    metrics &= cfg->metrics_enabled();
    tracing &= cfg->tracing_enabled();
    logs &= cfg->logs_enabled();
    debug = cfg->debug();
  }

  // ----------------------------------------------------------
  // Initialize signals
  //
  // Init order is NOT critical, but we prefer:
  //   Traces → Logs → Metrics
  // so logs can immediately attach span context.
  // ----------------------------------------------------------
  if (tracing) {
    InitTracing(cfg ? &cfg->tracing() : nullptr, global, debug);
  }

  if (logs) {
    InitLogging(cfg ? &cfg->logging() : nullptr, global, debug);
  }

  if (metrics) {
    InitMetrics(cfg ? &cfg->metrics() : nullptr, global, debug);
  }
#endif
}

// ------------------------------------------------------------
// ShutdownObservability
// ------------------------------------------------------------
void ShutdownObservability() {
#if !FLOWPIPE_ENABLE_OTEL
  return;
#else
  auto& state = GetOtelState();

  // ----------------------------------------------------------
  // Logs (flush first)
  // ----------------------------------------------------------
  if (state.logger_provider) {
    state.logger_provider->Shutdown();
    state.logger_provider.reset();
  }

  // ----------------------------------------------------------
  // Traces (flush spans)
  // ----------------------------------------------------------
  if (state.tracer_provider) {
    state.tracer_provider->Shutdown();
    state.tracer_provider.reset();
  }

  // ----------------------------------------------------------
  // Metrics (stop periodic readers last)
  // ----------------------------------------------------------
  if (state.meter_provider) {
    state.meter_provider->Shutdown();
    state.meter_provider.reset();
  }
#endif
}

}  // namespace flowpipe::observability
