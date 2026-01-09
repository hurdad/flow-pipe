package config

import "flag"

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

		runtimeNamespace = flag.String(
			"runtime-namespace",
			envOrDefault(
				"FLOW_PIPE_RUNTIME_NAMESPACE",
				envOrDefault("POD_NAMESPACE", "default"),
			),
			"namespace for runtime workloads",
		)
	)

	flag.Parse()

	return Config{
		EtcdEndpoints:    splitComma(*etcdURLs),
		OTLPEndpoint:     *otelEndpoint,
		ServiceName:      *serviceName,
		RuntimeNamespace: *runtimeNamespace,
	}
}
