#include "flowpipe/observability/metrics.h"

#include "flowpipe/observability/observability_state.h"

#if FLOWPIPE_ENABLE_OTEL

#include <chrono>
#include <memory>
#include <string>

// jemalloc
#include <jemalloc/jemalloc.h>

// ---- OpenTelemetry: Metrics (API)
#include <opentelemetry/metrics/provider.h>

// ---- OpenTelemetry: Metrics (SDK)
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>

// ---- OpenTelemetry: OTLP exporters
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>

namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace otlp = opentelemetry::exporter::otlp;

namespace flowpipe::observability {

// ------------------------------------------------------------
// jemalloc helper
// ------------------------------------------------------------
static uint64_t JemallocStat(const char* name) {
  uint64_t value = 0;
  size_t sz = sizeof(value);
  if (mallctl(name, &value, &sz, nullptr, 0) != 0) {
    return 0;
  }
  return value;
}

// ------------------------------------------------------------
// Periodic metric reader option mapping
// ------------------------------------------------------------
static metrics_sdk::PeriodicExportingMetricReaderOptions CreatePeriodicMetricReaderOptions(
    const flowpipe::v1::ObservabilityConfig::MetricsConfig& cfg, const GlobalDefaults& global,
    bool debug) {
  metrics_sdk::PeriodicExportingMetricReaderOptions opts;

  if (cfg.collection_interval_ms() > 0) {
    opts.export_interval_millis = std::chrono::milliseconds(cfg.collection_interval_ms());
  }

  if (cfg.min_collection_interval_ms() > 0 &&
      opts.export_interval_millis < std::chrono::milliseconds(cfg.min_collection_interval_ms())) {
    opts.export_interval_millis = std::chrono::milliseconds(cfg.min_collection_interval_ms());
  }

  if (debug) {
    opts.export_interval_millis = std::chrono::milliseconds(500);
  }

  opts.export_timeout_millis = std::chrono::milliseconds(500);
  return opts;
}

// ------------------------------------------------------------
// Init metrics
// ------------------------------------------------------------
void InitMetrics(const flowpipe::v1::ObservabilityConfig* cfg, const GlobalDefaults& global,
                 bool debug) {
  auto& state = GetOtelState();

  if (!cfg || state.meter_provider) {
    return;
  }

  const auto& metrics_cfg = cfg->metrics();

  // ----------------------------------------------------------
  // Cache runtime flags
  // ----------------------------------------------------------
  state.stage_metrics_enabled = metrics_cfg.stage_metrics_enabled();
  state.queue_metrics_enabled = metrics_cfg.queue_metrics_enabled();
  state.flow_metrics_enabled = metrics_cfg.flow_metrics_enabled();
  state.latency_histograms = metrics_cfg.latency_histograms_enabled();
  state.metrics_counters_only = metrics_cfg.counters_only();

  if (!state.stage_metrics_enabled && !state.queue_metrics_enabled && !state.flow_metrics_enabled) {
    return;
  }

  // ----------------------------------------------------------
  // Endpoint & transport
  // ----------------------------------------------------------
  std::string endpoint = global.otlp_endpoint;
  auto transport = flowpipe::v1::OTLP_TRANSPORT_GRPC;

  if (!cfg->otlp_endpoint().empty() && global.allow_endpoint_overrides) {
    endpoint = cfg->otlp_endpoint();
  }

  if (cfg->transport() != flowpipe::v1::OTLP_TRANSPORT_UNSPECIFIED) {
    transport = cfg->transport();
  }

  // ----------------------------------------------------------
  // Exporter
  // ----------------------------------------------------------
  std::unique_ptr<metrics_sdk::PushMetricExporter> exporter;

  if (transport == flowpipe::v1::OTLP_TRANSPORT_HTTP) {
    otlp::OtlpHttpMetricExporterOptions opts;
    opts.url = endpoint;
    exporter = otlp::OtlpHttpMetricExporterFactory::Create(opts);
  } else {
    otlp::OtlpGrpcMetricExporterOptions opts;
    opts.endpoint = endpoint;
    opts.use_ssl_credentials = global.otlp_use_ssl_credentials;
    exporter = otlp::OtlpGrpcMetricExporterFactory::Create(opts);
  }

  // ----------------------------------------------------------
  // Metric reader
  // ----------------------------------------------------------
  auto reader_opts = CreatePeriodicMetricReaderOptions(metrics_cfg, global, debug);

  auto reader =
      metrics_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), reader_opts);

  // ----------------------------------------------------------
  // Meter context
  // ----------------------------------------------------------
  auto context = metrics_sdk::MeterContextFactory::Create();
  context->AddMetricReader(std::move(reader));

  // ----------------------------------------------------------
  // Provider
  // ----------------------------------------------------------
  state.meter_provider = metrics_sdk::MeterProviderFactory::Create(std::move(context));

  std::shared_ptr<opentelemetry::metrics::MeterProvider> api_provider(
      std::move(state.meter_provider));

  opentelemetry::metrics::Provider::SetMeterProvider(api_provider);

  // ----------------------------------------------------------
  // jemalloc observable metrics
  // ----------------------------------------------------------
  {
    auto meter = api_provider->GetMeter("flowpipe.jemalloc");

    auto allocated = meter->CreateInt64ObservableGauge("flowpipe.jemalloc.allocated.bytes",
                                                       "jemalloc allocated bytes", "bytes");

    auto active = meter->CreateInt64ObservableGauge("flowpipe.jemalloc.active.bytes",
                                                    "jemalloc active bytes", "bytes");

    auto resident = meter->CreateInt64ObservableGauge("flowpipe.jemalloc.resident.bytes",
                                                      "jemalloc resident bytes", "bytes");

    auto mapped = meter->CreateInt64ObservableGauge("flowpipe.jemalloc.mapped.bytes",
                                                    "jemalloc mapped bytes", "bytes");
    auto nmalloc = meter->CreateInt64ObservableGauge(
        "flowpipe.jemalloc.nmalloc", "Total number of jemalloc allocations", "allocations");

    auto ndalloc = meter->CreateInt64ObservableGauge(
        "flowpipe.jemalloc.ndalloc", "Total number of jemalloc deallocations", "deallocations");

    state.jemalloc_instruments = {allocated, active, resident, mapped, nmalloc, ndalloc};

    allocated->AddCallback(
        [](opentelemetry::metrics::ObserverResult observer, void*) {
          auto observer_long = opentelemetry::nostd::get<
              opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
              observer);

          observer_long->Observe(static_cast<int64_t>(JemallocStat("stats.allocated")));
        },
        nullptr);

    active->AddCallback(
        [](opentelemetry::metrics::ObserverResult observer, void*) {
          auto observer_long = opentelemetry::nostd::get<
              opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
              observer);

          observer_long->Observe(static_cast<int64_t>(JemallocStat("stats.active")));
        },
        nullptr);

    resident->AddCallback(
        [](opentelemetry::metrics::ObserverResult observer, void*) {
          auto observer_long = opentelemetry::nostd::get<
              opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
              observer);

          observer_long->Observe(static_cast<int64_t>(JemallocStat("stats.resident")));
        },
        nullptr);

    mapped->AddCallback(
        [](opentelemetry::metrics::ObserverResult observer, void*) {
          auto observer_long = opentelemetry::nostd::get<
              opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
              observer);

          observer_long->Observe(static_cast<int64_t>(JemallocStat("stats.mapped")));
        },
        nullptr);

    nmalloc->AddCallback(
        [](opentelemetry::metrics::ObserverResult observer, void*) {
          auto observer_long = opentelemetry::nostd::get<
              opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
              observer);

          observer_long->Observe(static_cast<int64_t>(JemallocStat("stats.nmalloc")));
        },
        nullptr);

    ndalloc->AddCallback(
        [](opentelemetry::metrics::ObserverResult observer, void*) {
          auto observer_long = opentelemetry::nostd::get<
              opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
              observer);

          observer_long->Observe(static_cast<int64_t>(JemallocStat("stats.ndalloc")));
        },
        nullptr);
  }

  if (debug) {
    fprintf(stderr,
            "[otel] metrics enabled "
            "(stage=%d queue=%d flow=%d histograms=%d counters_only=%d)\n",
            state.stage_metrics_enabled, state.queue_metrics_enabled, state.flow_metrics_enabled,
            state.latency_histograms, state.metrics_counters_only);
  }
}

}  // namespace flowpipe::observability

#else  // FLOWPIPE_ENABLE_OTEL

namespace flowpipe::observability {

void InitMetrics(const flowpipe::v1::ObservabilityConfig*, const GlobalDefaults&, bool) {}

}  // namespace flowpipe::observability

#endif
