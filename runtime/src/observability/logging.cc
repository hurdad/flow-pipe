#include "flowpipe/observability/logging.h"

#if FLOWPIPE_ENABLE_OTEL


#include "opentelemetry/exporters/otlp/otlp_environment.h"
#include "opentelemetry/exporters/otlp/otlp_http.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/sdk/common/global_log_handler.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/sdk/logs/logger_provider.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/sdk/logs/provider.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/sdk/trace/processor.h"
#include "opentelemetry/sdk/trace/provider.h"
#include "opentelemetry/sdk/trace/simple_processor_factory.h"
#include "opentelemetry/sdk/trace/tracer_provider.h"
#include "opentelemetry/sdk/trace/tracer_provider_factory.h"
#include "opentelemetry/trace/tracer_provider.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h"
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include "opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h"
#include <opentelemetry/logs/provider.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/logs/simple_log_record_processor.h>

namespace logs_sdk = opentelemetry::sdk::logs;
namespace otlp = opentelemetry::exporter::otlp;

namespace flowpipe::observability {

void InitLogging(const flowpipe::v1::ObservabilityConfig::LoggingConfig* cfg,
                 const GlobalDefaults& global, bool /*debug*/) {
  if (!cfg)
    return;

  std::string endpoint = global.logging_endpoint;
  auto transport = flowpipe::v1::OTLP_TRANSPORT_GRPC;

  if (!cfg->otlp_endpoint().empty() && global.allow_endpoint_overrides) {
    endpoint = cfg->otlp_endpoint();
  }

  if (cfg->transport() != flowpipe::v1::OTLP_TRANSPORT_UNSPECIFIED) {
    transport = cfg->transport();
  }

  std::unique_ptr<logs_sdk::LogRecordExporter> exporter;

  if (transport == flowpipe::v1::OTLP_TRANSPORT_HTTP) {
    otlp::OtlpHttpLogRecordExporterOptions   opts;
    opts.url = endpoint;
    exporter = otlp::OtlpHttpLogRecordExporterFactory::Create(opts);
  } else {
    otlp::OtlpGrpcExporterOptions   opts;
    opts.endpoint = endpoint;
    //exporter = otlp::OtlpGrpcLogRecordExporterFactory::Create(opts);
  }

  std::unique_ptr<logs_sdk::LogRecordProcessor> processor;

  if (cfg->processor() == flowpipe::v1::ObservabilityConfig::LoggingConfig::LOG_PROCESSOR_SIMPLE) {
    processor = std::make_unique<logs_sdk::SimpleLogRecordProcessor>(std::move(exporter));
  } else {
    processor = std::make_unique<logs_sdk::BatchLogRecordProcessor>(std::move(exporter));
  }

  auto provider = std::make_shared<logs_sdk::LoggerProvider>(std::move(processor));

  //opentelemetry::logs::Provider::SetLoggerProvider(provider);
}

}  // namespace flowpipe::observability

#else

namespace flowpipe::observability {
void InitLogging(const flowpipe::v1::ObservabilityConfig::LoggingConfig*, const GlobalDefaults&,
                 bool) {}
}  // namespace flowpipe::observability

#endif
