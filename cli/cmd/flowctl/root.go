package main

import (
	"fmt"
	"os"

	"github.com/spf13/cobra"
)

var (
	apiAddr   string
	apiKey    string
	namespace string
	verbose   bool
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
