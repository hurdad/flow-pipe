package config

import (
	"os"
	"strings"
)

type Config struct {
	EtcdEndpoints []string
}

func Load() Config {
	endpoints := os.Getenv("FLOW_ETCD_ENDPOINTS")
	if endpoints == "" {
		endpoints = "http://localhost:2379"
	}

	return Config{
		EtcdEndpoints: strings.Split(endpoints, ","),
	}
}
