package config

// Config defines all runtime configuration for the flow-pipe API.
type Config struct {
	// gRPC server listen address (e.g. ":9090")
	GRPCAddr string

	// HTTP (grpc-gateway) listen address (e.g. ":8080")
	HTTPAddr string

	// Comma-separated etcd endpoints parsed into a slice
	EtcdEndpoints []string

	// OTLP endpoint used for OpenTelemetry exporters (e.g. "localhost:4317")
	OTLPEndpoint string

	// Logical service name used in observability signals
	ServiceName string

	// Whether observability exporters should be enabled
	ObservabilityEnabled bool

	// LogLevel sets the slog level (debug, info, warn, error).
	LogLevel string

	// AuthEnabled toggles API authentication checks.
	AuthEnabled bool

	// APIKey is the shared token used for API key authentication.
	APIKey string

	// HTTPTLSEnabled toggles TLS for the HTTP (grpc-gateway) server.
	HTTPTLSEnabled bool

	// HTTPTLSCertFile is the path to the TLS certificate for the HTTP server.
	HTTPTLSCertFile string

	// HTTPTLSKeyFile is the path to the TLS key for the HTTP server.
	HTTPTLSKeyFile string

	// GRPCTLSEnabled toggles TLS for the gRPC server.
	GRPCTLSEnabled bool

	// GRPCTLSCertFile is the path to the TLS certificate for the gRPC server.
	GRPCTLSCertFile string

	// GRPCTLSKeyFile is the path to the TLS key for the gRPC server.
	GRPCTLSKeyFile string

	// GRPCTLSServerName overrides the expected server name for gRPC TLS clients.
	GRPCTLSServerName string
}
