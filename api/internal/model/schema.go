package model

import (
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

type SchemaDefinition struct {
	RegistryID string                       `json:"registry_id"`
	Version    uint32                       `json:"version"`
	Format     flowpipev1.QueueSchemaFormat `json:"format"`
	RawSchema  []byte                       `json:"raw_schema"`
}
