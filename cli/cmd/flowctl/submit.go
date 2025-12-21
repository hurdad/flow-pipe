package main

import (
	"context"
	"fmt"
	"time"

	"github.com/hurdad/flow-pipe/cli/internal/loader"
	"github.com/hurdad/flow-pipe/pkg/flow/normalize"
	"github.com/hurdad/flow-pipe/pkg/flow/validate"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"

	"github.com/spf13/cobra"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

var (
	submitAPIAddr string
	submitTimeout time.Duration
)

var submitCmd = &cobra.Command{
	Use:   "submit <flow.yaml>",
	Short: "Submit a flow to the flow-pipe API",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		path := args[0]

		// 1. Load YAML → FlowSpec
		spec, err := loader.LoadFlowSpec(path)
		if err != nil {
			return err
		}

		// 2. Normalize + validate locally
		spec = normalize.Normalize(spec)
		if err := validate.Validate(spec); err != nil {
			return err
		}

		// 3. Connect to API
		ctx, cancel := context.WithTimeout(context.Background(), submitTimeout)
		defer cancel()

		conn, err := grpc.DialContext(
			ctx,
			submitAPIAddr,
			grpc.WithTransportCredentials(insecure.NewCredentials()),
			grpc.WithBlock(),
		)
		if err != nil {
			return fmt.Errorf("connect to api %s: %w", submitAPIAddr, err)
		}
		defer conn.Close()

		client := flowpipev1.NewFlowServiceClient(conn)

		// 4. Submit flow
		_, err = client.CreateFlow(ctx, &flowpipev1.CreateFlowRequest{
			Spec: spec,
		})
		if err != nil {
			return fmt.Errorf("submit flow: %w", err)
		}

		fmt.Printf(
			"✅ flow %q submitted successfully to %s\n",
			spec.Name,
			submitAPIAddr,
		)

		return nil
	},
}

func init() {
	submitCmd.Flags().StringVar(
		&submitAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	submitCmd.Flags().DurationVar(
		&submitTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	rootCmd.AddCommand(submitCmd)
}
