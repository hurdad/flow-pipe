#include "flowpipe/observability/metrics.h"

#include "flowpipe/observability/observability_state.h"

#if FLOWPIPE_ENABLE_OTEL

#include <chrono>
#include <memory>
#include <string>

// ---- OpenTelemetry: Metrics (API)
#include <opentelemetry/metrics/provider.h>

// ---- OpenTelemetry: Metrics (SDK)
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>

// ---- OpenTelemetry: OTLP exporters
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>

namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace otlp = opentelemetry::exporter::otlp;

namespace flowpipe::observability {

// ------------------------------------------------------------
// Periodic metric reader option mapping
// ------------------------------------------------------------
static metrics_sdk::PeriodicExportingMetricReaderOptions CreatePeriodicMetricReaderOptions(
    const flowpipe::v1::ObservabilityConfig::MetricsConfig& cfg, bool debug) {
  metrics_sdk::PeriodicExportingMetricReaderOptions opts;

  // Apply explicit intent
  if (cfg.collection_interval_ms() > 0)
    opts.export_interval_millis = std::chrono::milliseconds(cfg.collection_interval_ms());

  // Runtime default if not provided
  if (cfg.min_collection_interval_ms() > 0 &&
      opts.export_interval_millis < std::chrono::milliseconds(cfg.min_collection_interval_ms())) {
    opts.export_interval_millis = std::chrono::milliseconds(cfg.min_collection_interval_ms());
  }

  // Debug intent → faster export cadence
  if (debug) {
    opts.export_interval_millis = std::chrono::milliseconds(500);
  }

  // Conservative timeout (SDK default-safe)
  opts.export_timeout_millis = std::chrono::milliseconds(500);

  return opts;
}

// ------------------------------------------------------------
// Init metrics
// ------------------------------------------------------------
void InitMetrics(const flowpipe::v1::ObservabilityConfig::MetricsConfig* cfg,
                 const GlobalDefaults& global, bool debug) {
  auto& state = GetOtelState();

  if (!cfg || state.meter_provider)
    return;

  // ----------------------------------------------------------
  // Endpoint & transport resolution
  // ----------------------------------------------------------
  std::string endpoint = global.metrics_endpoint;
  auto transport = flowpipe::v1::OTLP_TRANSPORT_GRPC;

  if (!cfg->otlp_endpoint().empty() && global.allow_endpoint_overrides)
    endpoint = cfg->otlp_endpoint();

  if (cfg->transport() != flowpipe::v1::OTLP_TRANSPORT_UNSPECIFIED)
    transport = cfg->transport();

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
    exporter = otlp::OtlpGrpcMetricExporterFactory::Create(opts);
  }

  // ----------------------------------------------------------
  // Metric reader
  // ----------------------------------------------------------
  auto reader_opts = CreatePeriodicMetricReaderOptions(*cfg, debug);

  auto reader =
      metrics_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), reader_opts);

  // ----------------------------------------------------------
  // Meter context
  // ----------------------------------------------------------
  auto context = metrics_sdk::MeterContextFactory::Create();
  context->AddMetricReader(std::move(reader));

  // ----------------------------------------------------------
  // Provider (SDK → API)
  // ----------------------------------------------------------
  state.meter_provider = metrics_sdk::MeterProviderFactory::Create(std::move(context));
  std::shared_ptr<opentelemetry::metrics::MeterProvider> api_provider(
      std::move(state.meter_provider));
  opentelemetry::metrics::Provider::SetMeterProvider(api_provider);
}

}  // namespace flowpipe::observability

#else  // FLOWPIPE_ENABLE_OTEL

namespace flowpipe::observability {

void InitMetrics(const flowpipe::v1::ObservabilityConfig::MetricsConfig*, const GlobalDefaults&,
                 bool) {}

}  // namespace flowpipe::observability

#endif
