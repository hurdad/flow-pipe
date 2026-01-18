package main

import (
	"fmt"
	"os"
	"strconv"

	"github.com/spf13/cobra"
)

var (
	apiAddr           string
	apiKey            string
	namespace         string
	verbose           bool
	grpcTLSEnabled    bool
	grpcTLSCertFile   string
	grpcTLSServerName string
)

var rootCmd = &cobra.Command{
	Use:   "flowctl",
	Short: "flowctl manages flow-pipe flows",
	Long: `flowctl is the CLI for flow-pipe.

It can run flows locally or submit them to a flow-pipe API.`,
	SilenceUsage: true,
}

func Execute() error {
	return rootCmd.Execute()
}

func init() {
	rootCmd.PersistentFlags().StringVar(
		&apiAddr,
		"api",
		"",
		"Flow-pipe API address (e.g. https://flow-pipe.local)",
	)
	rootCmd.PersistentFlags().StringVar(
		&apiKey,
		"api-key",
		os.Getenv("FLOW_API_KEY"),
		"Flow-pipe API key (or set FLOW_API_KEY)",
	)
	rootCmd.PersistentFlags().BoolVar(
		&grpcTLSEnabled,
		"grpc-tls-enabled",
		envBoolDefault("FLOW_GRPC_TLS_ENABLED", false),
		"Enable TLS for gRPC connections (or set FLOW_GRPC_TLS_ENABLED)",
	)
	rootCmd.PersistentFlags().StringVar(
		&grpcTLSCertFile,
		"grpc-tls-cert",
		os.Getenv("FLOW_GRPC_TLS_CERT"),
		"Path to gRPC TLS certificate (or set FLOW_GRPC_TLS_CERT)",
	)
	rootCmd.PersistentFlags().StringVar(
		&grpcTLSServerName,
		"grpc-tls-server-name",
		os.Getenv("FLOW_GRPC_TLS_SERVER_NAME"),
		"Expected gRPC TLS server name (or set FLOW_GRPC_TLS_SERVER_NAME)",
	)

	rootCmd.PersistentFlags().StringVar(
		&namespace,
		"namespace",
		"default",
		"Kubernetes namespace",
	)

	rootCmd.PersistentFlags().BoolVarP(
		&verbose,
		"verbose",
		"v",
		false,
		"Enable verbose output",
	)

	rootCmd.SetOut(os.Stdout)
	rootCmd.SetErr(os.Stderr)
}

func debugf(format string, args ...any) {
	if verbose {
		fmt.Fprintf(os.Stderr, format+"\n", args...)
	}
}

func envBoolDefault(key string, fallback bool) bool {
	value, ok := os.LookupEnv(key)
	if !ok {
		return fallback
	}
	parsed, err := strconv.ParseBool(value)
	if err != nil {
		return fallback
	}
	return parsed
}
