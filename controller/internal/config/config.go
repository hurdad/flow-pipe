package config

import "time"

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

	// Whether observability exporters should be enabled
	ObservabilityEnabled bool

	// LogLevel sets the slog level (debug, info, warn, error).
	LogLevel string

	// Number of workers processing the reconciliation queue.
	WorkerCount int

	// Whether to enable leader election for HA controllers.
	LeaderElectionEnabled bool

	// Leader election resource name.
	LeaderElectionName string

	// Namespace for leader election lease.
	LeaderElectionNamespace string

	// Leader election lease duration.
	LeaderElectionLeaseDuration time.Duration

	// Leader election renew deadline.
	LeaderElectionRenewDeadline time.Duration

	// Leader election retry period.
	LeaderElectionRetryPeriod time.Duration
}
