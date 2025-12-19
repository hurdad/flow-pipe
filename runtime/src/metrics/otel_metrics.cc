#include "metrics/otel_metrics.h"

// OpenTelemetry API
#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/nostd/shared_ptr.h>

// OpenTelemetry SDK
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/resource/resource.h>

// OTLP exporter
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter.h>

#include <chrono>
#include <memory>

namespace otel_metrics = opentelemetry::metrics;
namespace otel_sdk_metrics = opentelemetry::sdk::metrics;
namespace otel_resource = opentelemetry::sdk::resource;
namespace otel_otlp = opentelemetry::exporter::otlp;

namespace flowpipe::metrics {

// ---- Global OTEL objects (kept alive for process lifetime) ----

static opentelemetry::nostd::shared_ptr<otel_metrics::MeterProvider> provider;
static opentelemetry::nostd::shared_ptr<otel_metrics::Meter> meter;

// Instruments
static otel_metrics::Counter<uint64_t>* flow_started = nullptr;
static otel_metrics::Counter<uint64_t>* flow_completed = nullptr;
static otel_metrics::Counter<uint64_t>* stage_processed = nullptr;
static otel_metrics::UpDownCounter<int64_t>* queue_depth = nullptr;

// -------------------------------------------------------------

void Metrics::Init(const std::string& service_name, const std::string& endpoint) {
  // ---- Exporter ----
  otel_otlp::OtlpGrpcMetricExporterOptions exporter_opts;
  exporter_opts.endpoint = endpoint;

  auto exporter = std::make_unique<otel_otlp::OtlpGrpcMetricExporter>(exporter_opts);

  // ---- Reader ----
  otel_sdk_metrics::PeriodicExportingMetricReaderOptions peroid_options;
  peroid_options.export_interval_millis = std::chrono::milliseconds{5000};

  auto reader = std::make_unique<otel_sdk_metrics::PeriodicExportingMetricReader>(
      std::move(exporter), peroid_options);

  // ---- Resource ----
  auto resource = otel_resource::Resource::Create({
      {"service.name", service_name},
      {"service.namespace", "flow-pipe"},
  });

  // // ---- SDK Provider ----
  // auto sdk_provider =
  //     std::make_shared<otel_sdk_metrics::MeterProvider>(resource);
  //
  // sdk_provider->AddMetricReader(std::move(reader));
  //
  // // IMPORTANT:
  // // OpenTelemetry API expects nostd::shared_ptr, not std::shared_ptr
  // provider = opentelemetry::nostd::shared_ptr<otel_metrics::MeterProvider>(
  //     sdk_provider);
  //
  // otel_metrics::Provider::SetMeterProvider(provider);

  // ---- Meter ----
  meter = provider->GetMeter("flowpipe.runtime", "1.0.0");

  // ---- Instruments ----
  // flow_started =
  //     &meter->CreateUInt64Counter(
  //         "flow_started_total",
  //         "Number of flows started");
  //
  // flow_completed =
  //     &meter->CreateUInt64Counter(
  //         "flow_completed_total",
  //         "Number of flows completed");
  //
  // stage_processed =
  //     &meter->CreateUInt64Counter(
  //         "stage_processed_total",
  //         "Number of stage executions");
  //
  // queue_depth =
  //     &meter->CreateUpDownCounter(
  //         "queue_depth",
  //         "Current depth of queues");
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
