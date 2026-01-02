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
	)

	flag.Parse()

	return Config{
		GRPCAddr:      *grpcAddr,
		HTTPAddr:      *httpAddr,
		EtcdEndpoints: splitComma(*etcdURLs),
		OTLPEndpoint:  *otelEndpoint,
		ServiceName:   *serviceName,
	}
}
