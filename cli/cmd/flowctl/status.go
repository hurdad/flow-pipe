package main

import (
	"context"
	"fmt"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/spf13/cobra"
)

var (
	statusAPIAddr string
	statusTimeout time.Duration
)

var statusCmd = &cobra.Command{
	Use:   "status <flow-name>",
	Short: "Get flow status from the flow-pipe API",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		name := args[0]

		ctx, cancel := context.WithTimeout(context.Background(), statusTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, statusAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewFlowServiceClient(conn)
		status, err := client.GetFlowStatus(ctx, &flowpipev1.GetFlowStatusRequest{Name: name})
		if err != nil {
			return fmt.Errorf("get flow status: %w", err)
		}

		return writeProto(cmd, status)
	},
}

func init() {
	statusCmd.Flags().StringVar(
		&statusAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	statusCmd.Flags().DurationVar(
		&statusTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	rootCmd.AddCommand(statusCmd)
}
