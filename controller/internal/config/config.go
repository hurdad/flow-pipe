package config

// Config defines runtime configuration for the flow-pipe controller.
type Config struct {
	// Etcd endpoints (desired state store)
	EtcdEndpoints []string

	// OTLP endpoint used for OpenTelemetry exporters (e.g. "localhost:4317")
	OTLPEndpoint string

	// Logical service name used in observability signals
	ServiceName string

	// Namespace for runtime workloads.
	RuntimeNamespace string
}
