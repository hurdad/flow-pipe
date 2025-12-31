#include "flowpipe/observability/metrics.h"

#if FLOWPIPE_ENABLE_OTEL

#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>

namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace otlp = opentelemetry::exporter::otlp;

namespace flowpipe::observability {

void InitMetrics(const flowpipe::v1::ObservabilityConfig::MetricsConfig* cfg,
                 const GlobalDefaults& global, bool /*debug*/) {
  // Start with global endpoint
  std::string endpoint = global.metrics_endpoint;

  // Default to gRPC transport
  auto transport = flowpipe::v1::OTLP_TRANSPORT_GRPC;

  // Apply flow overrides if allowed
  if (cfg) {
    if (!cfg->otlp_endpoint().empty() && global.allow_endpoint_overrides) {
      endpoint = cfg->otlp_endpoint();
    }
    transport = cfg->transport();
  }

  // Create exporter
//   std::unique_ptr<metrics_sdk::MetricExporter> exporter;
//
//   if (transport == flowpipe::v1::OTLP_TRANSPORT_HTTP) {
//     otlp::OtlpHttpMetricExporterOptions opts;
//     opts.url = endpoint;
//     exporter = std::make_unique<otlp::OtlpHttpMetricExporter>(opts);
//   } else {
//     otlp::OtlpGrpcMetricExporterOptions opts;
//     opts.endpoint = endpoint;
//     exporter = std::make_unique<otlp::OtlpGrpcMetricExporter>(opts);
//   }
//
//   // Periodic metric reader
//   auto reader = std::make_unique<metrics_sdk::PeriodicExportingMetricReader>(std::move(exporter));
//
//   // Install meter provider
//   auto provider = std::make_shared<metrics_sdk::MeterProvider>();
//   provider->AddMetricReader(std::move(reader));
//
//   opentelemetry::metrics::Provider::SetMeterProvider(provider);
 }

}  // namespace flowpipe::observability

#else

// No-op implementation when OTEL is disabled
namespace flowpipe::observability {
void InitMetrics(const flowpipe::v1::ObservabilityConfig::MetricsConfig*, const GlobalDefaults&,
                 bool) {}
}  // namespace flowpipe::observability

#endif
