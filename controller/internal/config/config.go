package config

// Config defines runtime configuration for the flow-pipe controller.
type Config struct {
	// Etcd endpoints (desired state store)
	EtcdEndpoints []string
}
