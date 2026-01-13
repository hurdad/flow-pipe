package main

import (
	"context"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"github.com/spf13/cobra"
)

var schemaCmd = &cobra.Command{
	Use:   "schema",
	Short: "Manage schema registry entries",
}

var (
	schemaCreateAPIAddr  string
	schemaCreateTimeout  time.Duration
	schemaCreateFormat   string
	schemaCreateFilePath string
)

var schemaCreateCmd = &cobra.Command{
	Use:   "create <registry-id>",
	Short: "Create a new schema version",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		registryID := args[0]
		format, err := parseQueueSchemaFormat(schemaCreateFormat)
		if err != nil {
			return err
		}

		payload, err := readSchemaPayload(schemaCreateFilePath)
		if err != nil {
			return err
		}

		ctx, cancel := context.WithTimeout(context.Background(), schemaCreateTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, schemaCreateAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewSchemaRegistryServiceClient(conn)
		resp, err := client.CreateSchema(ctx, &flowpipev1.CreateSchemaRequest{
			Schema: &flowpipev1.SchemaDefinition{
				RegistryId: registryID,
				Format:     format,
				RawSchema:  payload,
			},
		})
		if err != nil {
			return fmt.Errorf("create schema: %w", err)
		}

		return writeProto(cmd, resp)
	},
}

var (
	schemaGetAPIAddr string
	schemaGetTimeout time.Duration
)

var schemaGetCmd = &cobra.Command{
	Use:   "get <registry-id> <version>",
	Short: "Get a schema version",
	Args:  cobra.ExactArgs(2),
	RunE: func(cmd *cobra.Command, args []string) error {
		registryID := args[0]
		version, err := parseSchemaVersion(args[1])
		if err != nil {
			return err
		}

		ctx, cancel := context.WithTimeout(context.Background(), schemaGetTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, schemaGetAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewSchemaRegistryServiceClient(conn)
		resp, err := client.GetSchema(ctx, &flowpipev1.GetSchemaRequest{
			RegistryId: registryID,
			Version:    version,
		})
		if err != nil {
			return fmt.Errorf("get schema: %w", err)
		}

		return writeProto(cmd, resp)
	},
}

var (
	schemaListAPIAddr string
	schemaListTimeout time.Duration
)

var schemaListCmd = &cobra.Command{
	Use:   "list <registry-id>",
	Short: "List schema versions",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		registryID := args[0]

		ctx, cancel := context.WithTimeout(context.Background(), schemaListTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, schemaListAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewSchemaRegistryServiceClient(conn)
		resp, err := client.ListSchemaVersions(ctx, &flowpipev1.ListSchemaVersionsRequest{
			RegistryId: registryID,
		})
		if err != nil {
			return fmt.Errorf("list schema versions: %w", err)
		}

		return writeProto(cmd, resp)
	},
}

var (
	schemaDeleteAPIAddr string
	schemaDeleteTimeout time.Duration
)

var schemaDeleteCmd = &cobra.Command{
	Use:   "delete <registry-id>",
	Short: "Delete all schema versions",
	Args:  cobra.ExactArgs(1),
	RunE: func(cmd *cobra.Command, args []string) error {
		registryID := args[0]

		ctx, cancel := context.WithTimeout(context.Background(), schemaDeleteTimeout)
		defer cancel()

		conn, err := dialAPI(ctx, schemaDeleteAPIAddr)
		if err != nil {
			return err
		}
		defer conn.Close()

		client := flowpipev1.NewSchemaRegistryServiceClient(conn)
		resp, err := client.DeleteSchema(ctx, &flowpipev1.DeleteSchemaRequest{
			RegistryId: registryID,
		})
		if err != nil {
			return fmt.Errorf("delete schema: %w", err)
		}

		return writeProto(cmd, resp)
	},
}

func parseQueueSchemaFormat(value string) (flowpipev1.QueueSchemaFormat, error) {
	switch strings.ToLower(strings.TrimSpace(value)) {
	case "avro":
		return flowpipev1.QueueSchemaFormat_QUEUE_SCHEMA_FORMAT_AVRO, nil
	case "json", "json-schema", "jsonschema":
		return flowpipev1.QueueSchemaFormat_QUEUE_SCHEMA_FORMAT_JSON, nil
	case "protobuf", "proto":
		return flowpipev1.QueueSchemaFormat_QUEUE_SCHEMA_FORMAT_PROTOBUF, nil
	case "flatbuffers", "flatbuffer":
		return flowpipev1.QueueSchemaFormat_QUEUE_SCHEMA_FORMAT_FLATBUFFERS, nil
	case "parquet":
		return flowpipev1.QueueSchemaFormat_QUEUE_SCHEMA_FORMAT_PARQUET, nil
	default:
		return flowpipev1.QueueSchemaFormat_QUEUE_SCHEMA_FORMAT_UNSPECIFIED, fmt.Errorf(
			"unsupported schema format %q (use avro, json, protobuf, flatbuffers, parquet)",
			value,
		)
	}
}

func parseSchemaVersion(value string) (uint32, error) {
	version, err := strconv.ParseUint(value, 10, 32)
	if err != nil {
		return 0, fmt.Errorf("invalid schema version %q", value)
	}
	if version == 0 {
		return 0, fmt.Errorf("schema version must be greater than 0")
	}
	return uint32(version), nil
}

func readSchemaPayload(path string) ([]byte, error) {
	if path == "" {
		return nil, fmt.Errorf("schema file path is required")
	}
	if path == "-" {
		data, err := io.ReadAll(os.Stdin)
		if err != nil {
			return nil, fmt.Errorf("read schema from stdin: %w", err)
		}
		return data, nil
	}

	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("read schema file %s: %w", path, err)
	}
	return data, nil
}

func init() {
	schemaCreateCmd.Flags().StringVar(
		&schemaCreateAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	schemaCreateCmd.Flags().DurationVar(
		&schemaCreateTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	schemaCreateCmd.Flags().StringVar(
		&schemaCreateFormat,
		"format",
		"",
		"schema format (avro, json, protobuf, flatbuffers, parquet)",
	)

	schemaCreateCmd.Flags().StringVar(
		&schemaCreateFilePath,
		"schema-file",
		"",
		"path to schema file (use - for stdin)",
	)

	if err := schemaCreateCmd.MarkFlagRequired("format"); err != nil {
		panic(err)
	}

	if err := schemaCreateCmd.MarkFlagRequired("schema-file"); err != nil {
		panic(err)
	}

	schemaGetCmd.Flags().StringVar(
		&schemaGetAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	schemaGetCmd.Flags().DurationVar(
		&schemaGetTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	schemaListCmd.Flags().StringVar(
		&schemaListAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	schemaListCmd.Flags().DurationVar(
		&schemaListTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	schemaDeleteCmd.Flags().StringVar(
		&schemaDeleteAPIAddr,
		"api",
		"localhost:9090",
		"flow-pipe API address",
	)

	schemaDeleteCmd.Flags().DurationVar(
		&schemaDeleteTimeout,
		"timeout",
		5*time.Second,
		"API request timeout",
	)

	schemaCmd.AddCommand(schemaCreateCmd)
	schemaCmd.AddCommand(schemaGetCmd)
	schemaCmd.AddCommand(schemaListCmd)
	schemaCmd.AddCommand(schemaDeleteCmd)
	rootCmd.AddCommand(schemaCmd)
}
