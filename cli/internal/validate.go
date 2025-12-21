package internal

import (
	"fmt"

	"github.com/spf13/cobra"
)

var validateCmd = &cobra.Command{
	Use:   "validate <flow.yaml>",
	Short: "Validate a flow definition",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		flowPath := args[0]

		debugf("validating flow: %s", flowPath)

		// Phase 2:
		// - load YAML
		// - normalize
		// - semantic validation

		fmt.Printf("✓ %s is valid (not really, yet)\n", flowPath)
		return nil
	},
}

func init() {
	rootCmd.AddCommand(validateCmd)
}
