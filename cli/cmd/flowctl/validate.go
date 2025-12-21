package main

import (
	"fmt"
	"os"

	"github.com/hurdad/flow-pipe/cli/internal/loader"
	"github.com/hurdad/flow-pipe/pkg/flow/normalize"
	"github.com/hurdad/flow-pipe/pkg/flow/validate"
	"github.com/spf13/cobra"
)

var validateCmd = &cobra.Command{
	Use:   "validate <flow.yaml>",
	Short: "Validate a flow definition",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		path := args[0]

		spec, err := loader.LoadFlowSpec(path)
		if err != nil {
			return err
		}

		spec = normalize.Normalize(spec)

		if err := validate.Validate(spec); err != nil {
			fmt.Fprintln(os.Stderr, "❌ flow is invalid")
			return err
		}

		fmt.Println("✅ flow is valid")
		return nil
	},
}

func init() {
	rootCmd.AddCommand(validateCmd)
}
