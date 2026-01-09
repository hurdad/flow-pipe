package main

import (
	"context"
	"fmt"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/spf13/cobra"
)

var (
	listAPIAddr string
	listTimeout time.Duration
)

var listCmd = &cobra.Command{
	Use:   "list",
	Short: "List flows from the flow-pipe API",
	Args:  cobra.NoArgs,
	RunE: func(cmd *cobra.Command, args []string) error {
		ctx, cancel := context.WithTimeout(context.Background(), listTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, listAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewFlowServiceClient(conn)
		resp, err := client.ListFlows(ctx, &flowpipev1.ListFlowsRequest{})
		if err != nil {
			return fmt.Errorf("list flows: %w", err)
		}

		return writeProto(cmd, resp)
	},
}

func init() {
	listCmd.Flags().StringVar(
		&listAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	listCmd.Flags().DurationVar(
		&listTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	rootCmd.AddCommand(listCmd)
}
