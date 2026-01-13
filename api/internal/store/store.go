package store

import (
	"context"

	"github.com/hurdad/flow-pipe/api/internal/model"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

type Store interface {
	CreateFlow(ctx context.Context, spec *flowpipev1.FlowSpec) (*flowpipev1.Flow, error)
	UpdateFlow(ctx context.Context, name string, spec *flowpipev1.FlowSpec) (*flowpipev1.Flow, error)
	GetFlow(ctx context.Context, name string) (*flowpipev1.Flow, error)
	ListFlows(ctx context.Context) ([]*flowpipev1.Flow, error)
	DeleteFlow(ctx context.Context, name string) error
	GetFlowStatus(ctx context.Context, name string) (*flowpipev1.FlowStatus, error)
	RollbackFlow(ctx context.Context, name string, version uint64) (*flowpipev1.Flow, error)
}

type SchemaRegistryStore interface {
	CreateSchema(ctx context.Context, schema *model.SchemaDefinition) (*model.SchemaDefinition, error)
	GetSchema(ctx context.Context, registryID string, version uint32) (*model.SchemaDefinition, error)
	ListSchemaVersions(ctx context.Context, registryID string) ([]*model.SchemaDefinition, error)
	DeleteSchema(ctx context.Context, registryID string) error
}
