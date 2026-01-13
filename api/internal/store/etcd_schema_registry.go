package store

import (
	"context"
	"encoding/json"
	"fmt"
	"path"
	"sort"
	"strconv"

	"github.com/hurdad/flow-pipe/api/internal/model"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	clientv3 "go.etcd.io/etcd/client/v3"
)

// ============================================================
// Schema registry key layout
// ============================================================
//
// /flowpipe/schemas/<registry_id>/active
// /flowpipe/schemas/<registry_id>/versions/<version>
//

const schemaRootPrefix = "/flowpipe/schemas"

func schemaPrefix(registryID string) string {
	return path.Join(schemaRootPrefix, registryID)
}

func schemaActiveKey(registryID string) string {
	return path.Join(schemaPrefix(registryID), "active")
}

func schemaVersionKey(registryID string, version uint32) string {
	return path.Join(
		schemaPrefix(registryID),
		"versions",
		strconv.FormatUint(uint64(version), 10),
	)
}

func schemaVersionsPrefix(registryID string) string {
	return path.Join(schemaPrefix(registryID), "versions") + "/"
}

// ============================================================
// CreateSchema (version starts at 1)
// ============================================================

func (s *EtcdStore) CreateSchema(
	ctx context.Context,
	schema *model.SchemaDefinition,
) (*model.SchemaDefinition, error) {
	if schema == nil {
		return nil, fmt.Errorf("schema is nil")
	}
	if schema.RegistryID == "" {
		return nil, fmt.Errorf("schema registry id is required")
	}
	if schema.Format == flowpipev1.QueueSchemaFormat_QUEUE_SCHEMA_FORMAT_UNSPECIFIED {
		return nil, fmt.Errorf("schema format is required")
	}
	if len(schema.RawSchema) == 0 {
		return nil, fmt.Errorf("schema payload is required")
	}

	return s.createSchemaVersion(ctx, schema.RegistryID, schema)
}

// ============================================================
// Shared version-creation logic (CAS-safe)
// ============================================================

func (s *EtcdStore) createSchemaVersion(
	ctx context.Context,
	registryID string,
	schema *model.SchemaDefinition,
) (*model.SchemaDefinition, error) {
	resp, err := s.cli.Get(ctx, schemaActiveKey(registryID))
	if err != nil {
		return nil, err
	}

	var (
		prevVersion uint64
		nextVersion uint64 = 1
	)

	if len(resp.Kvs) > 0 {
		prevVersion, err = strconv.ParseUint(string(resp.Kvs[0].Value), 10, 64)
		if err != nil {
			return nil, fmt.Errorf("invalid active schema version")
		}
		nextVersion = prevVersion + 1
	}

	schema.Version = uint32(nextVersion)

	payload, err := json.Marshal(schema)
	if err != nil {
		return nil, err
	}

	txn := s.cli.Txn(ctx)

	if len(resp.Kvs) == 0 {
		txn = txn.If(
			clientv3.Compare(
				clientv3.CreateRevision(schemaActiveKey(registryID)),
				"=",
				0,
			),
		)
	} else {
		txn = txn.If(
			clientv3.Compare(
				clientv3.Value(schemaActiveKey(registryID)),
				"=",
				string(resp.Kvs[0].Value),
			),
		)
	}

	txn = txn.Then(
		clientv3.OpPut(schemaVersionKey(registryID, uint32(nextVersion)), string(payload)),
		clientv3.OpPut(schemaActiveKey(registryID), strconv.FormatUint(nextVersion, 10)),
	)

	res, err := txn.Commit()
	if err != nil {
		return nil, err
	}
	if !res.Succeeded {
		return nil, fmt.Errorf("schema %q modified concurrently", registryID)
	}

	return schema, nil
}

// ============================================================
// GetSchema
// ============================================================

func (s *EtcdStore) GetSchema(
	ctx context.Context,
	registryID string,
	version uint32,
) (*model.SchemaDefinition, error) {
	if registryID == "" {
		return nil, fmt.Errorf("schema registry id is required")
	}

	if version == 0 {
		activeResp, err := s.cli.Get(ctx, schemaActiveKey(registryID))
		if err != nil {
			return nil, err
		}
		if len(activeResp.Kvs) == 0 {
			return nil, fmt.Errorf("schema %q not found", registryID)
		}
		activeVersion, err := strconv.ParseUint(string(activeResp.Kvs[0].Value), 10, 64)
		if err != nil {
			return nil, fmt.Errorf("invalid active schema version")
		}
		version = uint32(activeVersion)
	}

	resp, err := s.cli.Get(ctx, schemaVersionKey(registryID, version))
	if err != nil {
		return nil, err
	}
	if len(resp.Kvs) == 0 {
		return nil, fmt.Errorf("schema %q version %d not found", registryID, version)
	}

	var schema model.SchemaDefinition
	if err := json.Unmarshal(resp.Kvs[0].Value, &schema); err != nil {
		return nil, err
	}

	return &schema, nil
}

// ============================================================
// ListSchemaVersions
// ============================================================

func (s *EtcdStore) ListSchemaVersions(
	ctx context.Context,
	registryID string,
) ([]*model.SchemaDefinition, error) {
	if registryID == "" {
		return nil, fmt.Errorf("schema registry id is required")
	}

	resp, err := s.cli.Get(ctx, schemaVersionsPrefix(registryID), clientv3.WithPrefix())
	if err != nil {
		return nil, err
	}

	schemas := make([]*model.SchemaDefinition, 0, len(resp.Kvs))
	for _, kv := range resp.Kvs {
		var schema model.SchemaDefinition
		if err := json.Unmarshal(kv.Value, &schema); err != nil {
			return nil, err
		}
		schemas = append(schemas, &schema)
	}

	sort.Slice(schemas, func(i, j int) bool {
		return schemas[i].Version < schemas[j].Version
	})

	return schemas, nil
}

// ============================================================
// DeleteSchema
// ============================================================

func (s *EtcdStore) DeleteSchema(
	ctx context.Context,
	registryID string,
) error {
	if registryID == "" {
		return fmt.Errorf("schema registry id is required")
	}

	_, err := s.cli.Delete(ctx, schemaPrefix(registryID), clientv3.WithPrefix())
	return err
}
