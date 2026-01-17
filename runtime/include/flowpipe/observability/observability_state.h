#pragma once

#include <memory>
#include <vector>

#if FLOWPIPE_ENABLE_OTEL

#include <opentelemetry/logs/logger_provider.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/observable_instrument.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/tracer_provider.h>

#endif  // FLOWPIPE_ENABLE_OTEL

namespace flowpipe::observability {

// ------------------------------------------------------------
// Global OpenTelemetry runtime state
// ------------------------------------------------------------
struct OtelState {
#if FLOWPIPE_ENABLE_OTEL
  // ----------------------------------------------------------
  // Providers (SDK ownership)
  // ----------------------------------------------------------
  std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracer_provider;
  std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meter_provider;
  std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> logger_provider;

  std::vector<opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObservableInstrument>>
      jemalloc_instruments;

  // ----------------------------------------------------------
  // Metrics runtime flags (cached from MetricsConfig)
  // ----------------------------------------------------------
  bool stage_metrics_enabled = false;
  bool queue_metrics_enabled = false;
  bool flow_metrics_enabled = false;

  bool latency_histograms = false;
  bool metrics_counters_only = false;

  // ----------------------------------------------------------
  // Tracing runtime flags (for symmetry / future use)
  // ----------------------------------------------------------
  bool stage_spans_enabled = false;
  bool queue_spans_enabled = false;
  bool record_spans_enabled = false;
#endif
};

// ------------------------------------------------------------
// Global state access
// ------------------------------------------------------------
OtelState& GetOtelState();

}  // namespace flowpipe::observability
