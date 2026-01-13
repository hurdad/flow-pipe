package service

import (
	"context"
	"log/slog"

	"github.com/hurdad/flow-pipe/api/internal/model"
	"github.com/hurdad/flow-pipe/api/internal/store"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"google.golang.org/protobuf/types/known/emptypb"
)

type SchemaRegistryServer struct {
	flowpipev1.UnimplementedSchemaRegistryServiceServer
	store store.SchemaRegistryStore
}

func NewSchemaRegistryServer(store store.SchemaRegistryStore) *SchemaRegistryServer {
	return &SchemaRegistryServer{store: store}
}

func (s *SchemaRegistryServer) CreateSchema(ctx context.Context, r *flowpipev1.CreateSchemaRequest) (*flowpipev1.SchemaDefinition, error) {
	schema := r.GetSchema()
	schemaID := ""
	if schema != nil {
		schemaID = schema.GetSchemaId()
	}
	slog.DebugContext(ctx, "create schema request", slog.String("schema_id", schemaID))

	created, err := s.store.CreateSchema(ctx, schemaToModel(schema))
	if err != nil {
		return nil, err
	}
	return modelToSchema(created), nil
}

func (s *SchemaRegistryServer) GetSchema(ctx context.Context, r *flowpipev1.GetSchemaRequest) (*flowpipev1.SchemaDefinition, error) {
	slog.DebugContext(ctx, "get schema request", slog.String("schema_id", r.GetSchemaId()), slog.Uint64("version", uint64(r.GetVersion())))
	schema, err := s.store.GetSchema(ctx, r.GetSchemaId(), r.GetVersion())
	if err != nil {
		return nil, err
	}
	return modelToSchema(schema), nil
}

func (s *SchemaRegistryServer) ListSchemaVersions(ctx context.Context, r *flowpipev1.ListSchemaVersionsRequest) (*flowpipev1.ListSchemaVersionsResponse, error) {
	slog.DebugContext(ctx, "list schema versions request", slog.String("schema_id", r.GetSchemaId()))
	schemas, err := s.store.ListSchemaVersions(ctx, r.GetSchemaId())
	if err != nil {
		return nil, err
	}
	return &flowpipev1.ListSchemaVersionsResponse{Schemas: modelsToSchemas(schemas)}, nil
}

func (s *SchemaRegistryServer) DeleteSchema(ctx context.Context, r *flowpipev1.DeleteSchemaRequest) (*emptypb.Empty, error) {
	slog.DebugContext(ctx, "delete schema request", slog.String("schema_id", r.GetSchemaId()))
	if err := s.store.DeleteSchema(ctx, r.GetSchemaId()); err != nil {
		return nil, err
	}
	return &emptypb.Empty{}, nil
}

func schemaToModel(schema *flowpipev1.SchemaDefinition) *model.SchemaDefinition {
	if schema == nil {
		return nil
	}
	return &model.SchemaDefinition{
		SchemaID:  schema.GetSchemaId(),
		Version:   schema.GetVersion(),
		Format:    schema.GetFormat(),
		RawSchema: schema.GetRawSchema(),
	}
}

func modelToSchema(schema *model.SchemaDefinition) *flowpipev1.SchemaDefinition {
	if schema == nil {
		return nil
	}
	return &flowpipev1.SchemaDefinition{
		SchemaId:  schema.SchemaID,
		Version:   schema.Version,
		Format:    schema.Format,
		RawSchema: schema.RawSchema,
	}
}

func modelsToSchemas(schemas []*model.SchemaDefinition) []*flowpipev1.SchemaDefinition {
	if len(schemas) == 0 {
		return nil
	}
	out := make([]*flowpipev1.SchemaDefinition, 0, len(schemas))
	for _, schema := range schemas {
		out = append(out, modelToSchema(schema))
	}
	return out
}
