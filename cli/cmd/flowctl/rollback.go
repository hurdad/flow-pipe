package main

import (
	"context"
	"fmt"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/spf13/cobra"
)

var (
	rollbackAPIAddr string
	rollbackTimeout time.Duration
	rollbackVersion uint64
)

var rollbackCmd = &cobra.Command{
	Use:   "rollback <flow-name>",
	Short: "Rollback a flow to a previous version",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		name := args[0]
		if rollbackVersion == 0 {
			return fmt.Errorf("version is required")
		}

		ctx, cancel := context.WithTimeout(context.Background(), rollbackTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, rollbackAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewFlowServiceClient(conn)
		flow, err := client.RollbackFlow(ctx, &flowpipev1.RollbackFlowRequest{
			Name:    name,
			Version: rollbackVersion,
		})
		if err != nil {
			return fmt.Errorf("rollback flow: %w", err)
		}

		return writeProto(cmd, flow)
	},
}

func init() {
	rollbackCmd.Flags().StringVar(
		&rollbackAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	rollbackCmd.Flags().DurationVar(
		&rollbackTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	rollbackCmd.Flags().Uint64Var(
		&rollbackVersion,
		"version",
		0,
		"flow version to roll back to",
	)

	rootCmd.AddCommand(rollbackCmd)
}
