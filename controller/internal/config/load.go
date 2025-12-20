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
	)

	flag.Parse()

	return Config{
		EtcdEndpoints: splitComma(*etcdURLs),
	}
}
