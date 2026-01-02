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
}
