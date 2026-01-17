package config

import (
	"flag"
	"time"
)

// Load parses flags + env vars and returns Config.
func Load() Config {
	var (
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
			envOrDefault("OTEL_SERVICE_NAME", "flow-controller"),
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

		workerCount = flag.Int(
			"worker-count",
			envIntOrDefault("FLOW_CONTROLLER_WORKER_COUNT", 1),
			"number of workers processing the reconciliation queue",
		)

		runtimeNamespace = flag.String(
			"runtime-namespace",
			envOrDefault(
				"FLOW_PIPE_RUNTIME_NAMESPACE",
				envOrDefault("POD_NAMESPACE", "default"),
			),
			"namespace for runtime workloads",
		)

		leaderElectionEnabled = flag.Bool(
			"leader-election-enabled",
			envBoolOrDefault("FLOW_CONTROLLER_LEADER_ELECTION_ENABLED", true),
			"enable leader election for HA controllers",
		)

		leaderElectionNamespace = flag.String(
			"leader-election-namespace",
			envOrDefault("FLOW_CONTROLLER_LEADER_ELECTION_NAMESPACE", envOrDefault("POD_NAMESPACE", "default")),
			"namespace for the leader election lease",
		)

		leaderElectionName = flag.String(
			"leader-election-name",
			envOrDefault("FLOW_CONTROLLER_LEADER_ELECTION_NAME", "flow-controller"),
			"name of the leader election lease",
		)

		leaderElectionLeaseDuration = flag.Duration(
			"leader-election-lease-duration",
			envDurationOrDefault("FLOW_CONTROLLER_LEADER_ELECTION_LEASE_DURATION", 30*time.Second),
			"leader election lease duration",
		)

		leaderElectionRenewDeadline = flag.Duration(
			"leader-election-renew-deadline",
			envDurationOrDefault("FLOW_CONTROLLER_LEADER_ELECTION_RENEW_DEADLINE", 20*time.Second),
			"leader election renew deadline",
		)

		leaderElectionRetryPeriod = flag.Duration(
			"leader-election-retry-period",
			envDurationOrDefault("FLOW_CONTROLLER_LEADER_ELECTION_RETRY_PERIOD", 5*time.Second),
			"leader election retry period",
		)
	)

	flag.Parse()

	workers := *workerCount
	if workers < 1 {
		workers = 1
	}

	return Config{
		EtcdEndpoints:               splitComma(*etcdURLs),
		OTLPEndpoint:                *otelEndpoint,
		ServiceName:                 *serviceName,
		RuntimeNamespace:            *runtimeNamespace,
		ObservabilityEnabled:        *observabilityEnabled,
		LogLevel:                    *logLevel,
		WorkerCount:                 workers,
		LeaderElectionEnabled:       *leaderElectionEnabled,
		LeaderElectionName:          *leaderElectionName,
		LeaderElectionNamespace:     *leaderElectionNamespace,
		LeaderElectionLeaseDuration: *leaderElectionLeaseDuration,
		LeaderElectionRenewDeadline: *leaderElectionRenewDeadline,
		LeaderElectionRetryPeriod:   *leaderElectionRetryPeriod,
	}
}
