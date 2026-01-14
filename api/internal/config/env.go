package config

import (
	"os"
	"strings"
)

func envOrDefault(key, def string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return def
}

func envBoolOrDefault(key string, def bool) bool {
	if v := os.Getenv(key); v != "" {
		switch strings.ToLower(v) {
		case "1", "true", "t", "yes", "y":
			return true
		case "0", "false", "f", "no", "n":
			return false
		default:
			return def
		}
	}
	return def
}
