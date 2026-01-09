package main

import (
	"context"
	"fmt"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/spf13/cobra"
)

var (
	stopAPIAddr string
	stopTimeout time.Duration
)

var stopCmd = &cobra.Command{
	Use:     "stop <flow-name>",
	Aliases: []string{"remove", "delete"},
	Short:   "Stop (delete) a flow from the flow-pipe API",
	Args:    cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		name := args[0]

		ctx, cancel := context.WithTimeout(context.Background(), stopTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, stopAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewFlowServiceClient(conn)

		_, err = client.DeleteFlow(ctx, &flowpipev1.DeleteFlowRequest{
			Name: name,
		})
		if err != nil {
			return fmt.Errorf("delete flow: %w", err)
		}

		fmt.Printf("✅ flow %q deleted from %s\n", name, stopAPIAddr)
		return nil
	},
}

func init() {
	stopCmd.Flags().StringVar(
		&stopAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	stopCmd.Flags().DurationVar(
		&stopTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	rootCmd.AddCommand(stopCmd)
}
