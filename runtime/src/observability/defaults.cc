#include "flowpipe/observability/defaults.h"

#include <cstdlib>  // std::getenv
#include <string>

namespace flowpipe::observability {

// Read boolean env var with fallback
static bool GetEnvBool(const char* key, bool def) {
  if (const char* v = std::getenv(key)) {
    std::string s(v);
    return s == "1" || s == "true" || s == "TRUE";
  }
  return def;
}

// Read string env var with fallback
static std::string GetEnvString(const char* key, const std::string& def) {
  if (const char* v = std::getenv(key)) {
    return std::string(v);
  }
  return def;
}

GlobalDefaults LoadFromEnv() {
  GlobalDefaults cfg;

  // Master signal enablement
  cfg.observability_enabled = GetEnvBool("FLOWPIPE_OBSERVABILITY_ENABLED", false);
  cfg.metrics_enabled = GetEnvBool("FLOWPIPE_METRICS_ENABLED", true);

  cfg.tracing_enabled = GetEnvBool("FLOWPIPE_TRACING_ENABLED", false);

  cfg.logs_enabled = GetEnvBool("FLOWPIPE_LOGS_ENABLED", false);

  if (!cfg.observability_enabled) {
    cfg.metrics_enabled = false;
    cfg.tracing_enabled = false;
    cfg.logs_enabled = false;
  }

  // Default OTLP endpoint
  cfg.otlp_endpoint = GetEnvString("FLOWPIPE_OTEL_ENDPOINT",
                                   GetEnvString("OTEL_EXPORTER_OTLP_ENDPOINT", "localhost:4317"));

  // OTLP gRPC SSL/TLS credentials selection
  cfg.otlp_use_ssl_credentials = false;
  if (const char* insecure = std::getenv("OTEL_EXPORTER_OTLP_INSECURE")) {
    std::string s(insecure);
    if (s == "1" || s == "true" || s == "TRUE") {
      cfg.otlp_use_ssl_credentials = false;
    } else if (s == "0" || s == "false" || s == "FALSE") {
      cfg.otlp_use_ssl_credentials = true;
    }
  }
  if (const char* v = std::getenv("FLOWPIPE_OTEL_USE_SSL_CREDENTIALS")) {
    cfg.otlp_use_ssl_credentials =
        GetEnvBool("FLOWPIPE_OTEL_USE_SSL_CREDENTIALS", cfg.otlp_use_ssl_credentials);
  }

  // Policy: can flows override endpoints?
  cfg.allow_endpoint_overrides = GetEnvBool("FLOWPIPE_ALLOW_FLOW_ENDPOINT_OVERRIDES", false);

  return cfg;
}

}  // namespace flowpipe::observability
