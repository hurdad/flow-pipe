package internal

import (
	"fmt"

	"github.com/spf13/cobra"
)

var runCmd = &cobra.Command{
	Use:   "run <flow.yaml>",
	Short: "Run a flow locally",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		flowPath := args[0]

		debugf("running flow locally: %s", flowPath)

		// Phase 2:
		// - load YAML
		// - normalize
		// - validate
		// - convert to protobuf
		// - exec flow_runtime

		fmt.Printf("flowctl run %s (not implemented yet)\n", flowPath)
		return nil
	},
}

func init() {
	rootCmd.AddCommand(runCmd)
}
