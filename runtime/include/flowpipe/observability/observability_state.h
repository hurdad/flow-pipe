#pragma once

#include <memory>

#if FLOWPIPE_ENABLE_OTEL
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#endif

namespace flowpipe::observability {

// Owns all OTEL providers for process lifetime
struct OtelState {
#if FLOWPIPE_ENABLE_OTEL
  std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> tracer_provider;

  std::shared_ptr<opentelemetry::sdk::metrics::MeterProvider> meter_provider;

  std::shared_ptr<opentelemetry::sdk::logs::LoggerProvider> logger_provider;
#endif
};

// Accessor for global OTEL state
OtelState& GetOtelState();

}  // namespace flowpipe::observability
