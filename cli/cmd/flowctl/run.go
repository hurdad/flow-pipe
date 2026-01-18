package main

import (
	"fmt"
	"os"
	"os/exec"

	"github.com/hurdad/flow-pipe/cli/internal/loader"
	"github.com/hurdad/flow-pipe/pkg/flow/normalize"
	"github.com/hurdad/flow-pipe/pkg/flow/validate"

	"github.com/spf13/cobra"
)

var runCmd = &cobra.Command{
	Use:   "run <flow.yaml>",
	Short: "Run a flow locally using the flow runtime",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		path := args[0]

		// 1. Load YAML → FlowSpec
		spec, err := loader.LoadFlowSpec(path)
		if err != nil {
			return err
		}

		// 2. Normalize + validate
		spec = normalize.Normalize(spec)

		if err := validate.Validate(spec); err != nil {
			return err
		}

		// 3. Execute runtime
		runtime := "flow_runtime"

		c := exec.Command(runtime, path)
		c.Stdin = os.Stdin
		c.Stdout = os.Stdout
		c.Stderr = os.Stderr

		if err := c.Run(); err != nil {
			return fmt.Errorf("run flow runtime: %w", err)
		}

		return nil
	},
}

func init() {
	rootCmd.AddCommand(runCmd)
}
