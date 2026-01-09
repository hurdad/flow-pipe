package main

import (
	"context"
	"fmt"
	"time"

	"github.com/hurdad/flow-pipe/cli/internal/loader"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/hurdad/flow-pipe/pkg/flow/normalize"
	"github.com/hurdad/flow-pipe/pkg/flow/validate"

	"github.com/spf13/cobra"
)

var (
	updateAPIAddr string
	updateTimeout time.Duration
	updateName    string
)

var updateCmd = &cobra.Command{
	Use:   "update <flow.yaml>",
	Short: "Update a flow on the flow-pipe API",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		path := args[0]

		spec, err := loader.LoadFlowSpec(path)
		if err != nil {
			return err
		}

		spec = normalize.Normalize(spec)
		if err := validate.Validate(spec); err != nil {
			return err
		}

		name := updateName
		if name == "" {
			name = spec.Name
		}
		if name == "" {
			return fmt.Errorf("flow name is required")
		}

		ctx, cancel := context.WithTimeout(context.Background(), updateTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, updateAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewFlowServiceClient(conn)

		_, err = client.UpdateFlow(ctx, &flowpipev1.UpdateFlowRequest{
			Name: name,
			Spec: spec,
		})
		if err != nil {
			return fmt.Errorf("update flow: %w", err)
		}

		fmt.Printf("✅ flow %q updated successfully on %s\n", name, updateAPIAddr)
		return nil
	},
}

func init() {
	updateCmd.Flags().StringVar(
		&updateAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	updateCmd.Flags().DurationVar(
		&updateTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	updateCmd.Flags().StringVar(
		&updateName,
		"name",
		"",
		"flow name to update (defaults to the spec name)",
	)

	rootCmd.AddCommand(updateCmd)
}
