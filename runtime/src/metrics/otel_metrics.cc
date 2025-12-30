#include "metrics/otel_metrics.h"

#include <chrono>

#ifdef FLOWPIPE_ENABLE_OTEL
// OpenTelemetry API
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/nostd/shared_ptr.h>

// OpenTelemetry SDK
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/resource/resource.h>

// OTLP exporter
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>

namespace otel_metrics = opentelemetry::metrics;
namespace otel_sdk_metrics = opentelemetry::sdk::metrics;
namespace otel_otlp = opentelemetry::exporter::otlp;
namespace otel_resource = opentelemetry::sdk::resource;

namespace flowpipe::metrics {

namespace {

// ---- Global OTEL objects (kept alive for process lifetime) ----

opentelemetry::nostd::shared_ptr<otel_metrics::MeterProvider> provider;
opentelemetry::nostd::shared_ptr<otel_metrics::Meter> meter;

// Instruments
opentelemetry::nostd::shared_ptr<otel_metrics::Counter<uint64_t>> flow_started;
opentelemetry::nostd::shared_ptr<otel_metrics::Counter<uint64_t>> flow_completed;
opentelemetry::nostd::shared_ptr<otel_metrics::Counter<uint64_t>> stage_processed;
opentelemetry::nostd::shared_ptr<otel_metrics::UpDownCounter<int64_t>> queue_depth;

otel_resource::Resource CreateResource(const std::string& service_name) {
  return otel_resource::Resource::Create({
      {"service.name", service_name},
      {"service.namespace", "flow-pipe"},
  });
}

}  // namespace

// -------------------------------------------------------------

void Metrics::Init(const std::string& service_name, const std::string& endpoint) {
  // ---- Exporter ----
  otel_otlp::OtlpGrpcMetricExporterOptions exporter_opts;
  exporter_opts.endpoint = endpoint;
  exporter_opts.use_ssl_credentials = false;

  auto exporter = otel_otlp::OtlpGrpcMetricExporterFactory::Create(exporter_opts);

  // ---- Reader ----
  otel_sdk_metrics::PeriodicExportingMetricReaderOptions period_options;
  period_options.export_interval_millis = std::chrono::milliseconds{5000};

  auto reader = std::make_unique<otel_sdk_metrics::PeriodicExportingMetricReader>(
      std::move(exporter), period_options);

  // ---- Meter provider ----
  auto sdk_provider = std::make_shared<otel_sdk_metrics::MeterProvider>();
  sdk_provider->SetResource(CreateResource(service_name));
  sdk_provider->AddMetricReader(std::move(reader));

  provider = opentelemetry::nostd::shared_ptr<otel_metrics::MeterProvider>(sdk_provider);
  otel_metrics::Provider::SetMeterProvider(provider);

  // ---- Meter ----
  meter = provider->GetMeter("flowpipe.runtime", "1.0.0");

  // ---- Instruments ----
  flow_started = meter->CreateUInt64Counter(
      "flow_started_total", "Number of flows started");

  flow_completed = meter->CreateUInt64Counter(
      "flow_completed_total", "Number of flows completed");

  stage_processed = meter->CreateUInt64Counter(
      "stage_processed_total", "Number of stage executions");

  queue_depth = meter->CreateInt64UpDownCounter(
      "queue_depth", "Current depth of queues");
}

void Metrics::Shutdown() {
  // Order matters
  flow_started = nullptr;
  flow_completed = nullptr;
  stage_processed = nullptr;
  queue_depth = nullptr;

  meter = nullptr;
  provider = nullptr;
}

// ---- Metric helpers (safe no-ops if not initialized) ----

void Metrics::FlowStarted() {
  if (flow_started)
    flow_started->Add(1);
}

void Metrics::FlowCompleted() {
  if (flow_completed)
    flow_completed->Add(1);
}

void Metrics::StageProcessed(const std::string& stage) {
  if (stage_processed)
    stage_processed->Add(1, {{"stage", stage}});
}

void Metrics::QueueDepth(const std::string& queue, int64_t depth) {
  if (queue_depth)
    queue_depth->Add(depth, {{"queue", queue}});
}

}  // namespace flowpipe::metrics

#else  // FLOWPIPE_ENABLE_OTEL

namespace flowpipe::metrics {

void Metrics::Init(const std::string&, const std::string&) {}

void Metrics::Shutdown() {}

void Metrics::FlowStarted() {}

void Metrics::FlowCompleted() {}

void Metrics::StageProcessed(const std::string&) {}

void Metrics::QueueDepth(const std::string&, int64_t) {}

}  // namespace flowpipe::metrics

#endif  // FLOWPIPE_ENABLE_OTEL
