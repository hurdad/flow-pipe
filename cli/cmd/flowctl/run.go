package main

import (
	"fmt"
	"os"
	"os/exec"

	"github.com/hurdad/flow-pipe/cli/internal/loader"
	"github.com/hurdad/flow-pipe/pkg/flow/normalize"
	"github.com/hurdad/flow-pipe/pkg/flow/validate"
	"google.golang.org/protobuf/encoding/protojson"

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

		// 3. Encode FlowSpec → JSON
		data, err := protojson.MarshalOptions{
			EmitUnpopulated: true,
			UseProtoNames:   true,
		}.Marshal(spec)
		if err != nil {
			return fmt.Errorf("encode flow spec: %w", err)
		}

		// 4. Execute runtime
		runtime := "flow_runtime"

		c := exec.Command(runtime)
		c.Stdin = os.Stdin
		c.Stdout = os.Stdout
		c.Stderr = os.Stderr

		stdin, err := c.StdinPipe()
		if err != nil {
			return err
		}

		if err := c.Start(); err != nil {
			return err
		}

		// Send FlowSpec JSON to runtime stdin
		if _, err := stdin.Write(data); err != nil {
			return err
		}
		stdin.Close()

		return c.Wait()
	},
}

func init() {
	rootCmd.AddCommand(runCmd)
}
