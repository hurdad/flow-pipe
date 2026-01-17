package config

import "flag"

// Load parses flags + env vars and returns a Config.
func Load() Config {
	var (
		grpcAddr = flag.String(
			"grpc-addr",
			envOrDefault("FLOW_GRPC_ADDR", ":9090"),
			"gRPC listen address",
		)

		httpAddr = flag.String(
			"http-addr",
			envOrDefault("FLOW_HTTP_ADDR", ":8080"),
			"HTTP (grpc-gateway) listen address",
		)

		etcdURLs = flag.String(
			"etcd",
			envOrDefault("ETCD_ENDPOINTS", "http://127.0.0.1:2379"),
			"comma-separated etcd endpoints",
		)

		otelEndpoint = flag.String(
			"otel-endpoint",
			envOrDefault("OTEL_EXPORTER_OTLP_ENDPOINT", "localhost:4317"),
			"OTLP collector endpoint (host:port)",
		)

		serviceName = flag.String(
			"service-name",
			envOrDefault("OTEL_SERVICE_NAME", "flow-api"),
			"logical service name for observability signals",
		)

		observabilityEnabled = flag.Bool(
			"observability-enabled",
			envBoolOrDefault("FLOW_OBSERVABILITY_ENABLED", false),
			"enable OpenTelemetry exporters",
		)

		logLevel = flag.String(
			"log-level",
			envOrDefault("FLOW_LOG_LEVEL", "info"),
			"log level (debug, info, warn, error)",
		)

		authEnabled = flag.Bool(
			"auth-enabled",
			envBoolOrDefault("FLOW_AUTH_ENABLED", false),
			"enable API authentication",
		)

		apiKey = flag.String(
			"api-key",
			envOrDefault("FLOW_API_KEY", ""),
			"shared API key for authentication",
		)

		httpTLSEnabled = flag.Bool(
			"http-tls-enabled",
			envBoolOrDefault("FLOW_HTTP_TLS_ENABLED", false),
			"enable TLS for the HTTP server",
		)

		httpTLSCertFile = flag.String(
			"http-tls-cert",
			envOrDefault("FLOW_HTTP_TLS_CERT", ""),
			"path to the TLS certificate for the HTTP server",
		)

		httpTLSKeyFile = flag.String(
			"http-tls-key",
			envOrDefault("FLOW_HTTP_TLS_KEY", ""),
			"path to the TLS key for the HTTP server",
		)

		grpcTLSEnabled = flag.Bool(
			"grpc-tls-enabled",
			envBoolOrDefault("FLOW_GRPC_TLS_ENABLED", false),
			"enable TLS for the gRPC server",
		)

		grpcTLSCertFile = flag.String(
			"grpc-tls-cert",
			envOrDefault("FLOW_GRPC_TLS_CERT", ""),
			"path to the TLS certificate for the gRPC server",
		)

		grpcTLSKeyFile = flag.String(
			"grpc-tls-key",
			envOrDefault("FLOW_GRPC_TLS_KEY", ""),
			"path to the TLS key for the gRPC server",
		)

		grpcTLSServerName = flag.String(
			"grpc-tls-server-name",
			envOrDefault("FLOW_GRPC_TLS_SERVER_NAME", ""),
			"expected server name for gRPC TLS clients",
		)
	)

	flag.Parse()

	return Config{
		GRPCAddr:             *grpcAddr,
		HTTPAddr:             *httpAddr,
		EtcdEndpoints:        splitComma(*etcdURLs),
		OTLPEndpoint:         *otelEndpoint,
		ServiceName:          *serviceName,
		ObservabilityEnabled: *observabilityEnabled,
		LogLevel:             *logLevel,
		AuthEnabled:          *authEnabled,
		APIKey:               *apiKey,
		HTTPTLSEnabled:       *httpTLSEnabled,
		HTTPTLSCertFile:      *httpTLSCertFile,
		HTTPTLSKeyFile:       *httpTLSKeyFile,
		GRPCTLSEnabled:       *grpcTLSEnabled,
		GRPCTLSCertFile:      *grpcTLSCertFile,
		GRPCTLSKeyFile:       *grpcTLSKeyFile,
		GRPCTLSServerName:    *grpcTLSServerName,
	}
}
