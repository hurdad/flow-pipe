package main

import (
	"context"
	"fmt"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/spf13/cobra"
)

var (
	getAPIAddr string
	getTimeout time.Duration
)

var getCmd = &cobra.Command{
	Use:   "get <flow-name>",
	Short: "Get a flow from the flow-pipe API",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		name := args[0]

		ctx, cancel := context.WithTimeout(context.Background(), getTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, getAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewFlowServiceClient(conn)
		flow, err := client.GetFlow(ctx, &flowpipev1.GetFlowRequest{Name: name})
		if err != nil {
			return fmt.Errorf("get flow: %w", err)
		}

		return writeProto(cmd, flow)
	},
}

func init() {
	getCmd.Flags().StringVar(
		&getAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	getCmd.Flags().DurationVar(
		&getTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	rootCmd.AddCommand(getCmd)
}
