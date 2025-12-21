package internal

import (
	"fmt"

	"github.com/spf13/cobra"
)

var submitCmd = &cobra.Command{
	Use:   "submit <flow.yaml>",
	Short: "Submit a flow to the flow-pipe API",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		flowPath := args[0]

		if apiAddr == "" {
			return fmt.Errorf("--api is required for submit")
		}

		debugf("submitting flow: %s", flowPath)
		debugf("api: %s namespace: %s", apiAddr, namespace)

		// Phase 2:
		// - load YAML
		// - normalize
		// - validate
		// - convert to protobuf
		// - call API

		fmt.Printf(
			"flowctl submit %s → %s (not implemented yet)\n",
			flowPath,
			apiAddr,
		)
		return nil
	},
}

func init() {
	rootCmd.AddCommand(submitCmd)
}
