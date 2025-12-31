#include "flowpipe/observability/observability.h"

#include "flowpipe/observability/defaults.h"

#if FLOWPIPE_ENABLE_OTEL
#include "flowpipe/observability/logging.h"
#include "flowpipe/observability/metrics.h"
#include "flowpipe/observability/tracing.h"
#endif

namespace flowpipe::observability {

void InitFromProto(const flowpipe::v1::ObservabilityConfig* cfg) {
#if !FLOWPIPE_ENABLE_OTEL
  // OTEL compiled out: do nothing
  (void)cfg;
  return;
#else
  // Load deployment-level policy
  GlobalDefaults global = LoadFromEnv();

  // Start with global enablement
  bool metrics = global.metrics_enabled;
  bool tracing = global.tracing_enabled;
  bool logs = global.logs_enabled;

  // Debug intent defaults to false
  bool debug = false;

  // Apply flow-level intent if present
  if (cfg) {
    metrics &= cfg->metrics_enabled();
    tracing &= cfg->tracing_enabled();
    logs &= cfg->logs_enabled();
    debug = cfg->debug();
  }

  // Initialize tracing if allowed
  if (tracing) {
    InitTracing(cfg ? &cfg->tracing() : nullptr, global, debug);
  }

  // Initialize metrics if allowed
  if (metrics) {
    InitMetrics(cfg ? &cfg->metrics() : nullptr, global, debug);
  }

  // Initialize logging if allowed
  if (logs) {
    InitLogging(cfg ? &cfg->logging() : nullptr, global, debug);
  }
#endif
}

}  // namespace flowpipe::observability
