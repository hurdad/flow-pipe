package store

import (
	"context"
	"fmt"
	"path"
	"strconv"
	"strings"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/proto"
)

// ============================================================
// Etcd-backed Store
// ============================================================

type EtcdStore struct {
	cli *clientv3.Client
}

func NewEtcd(endpoints []string) (*EtcdStore, error) {
	cli, err := clientv3.New(clientv3.Config{
		Endpoints:   endpoints,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		return nil, err
	}

	return &EtcdStore{cli: cli}, nil
}

func (s *EtcdStore) Close() error {
	return s.cli.Close()
}

// ============================================================
// Key layout
// ============================================================
//
// /flowpipe/flows/<name>/active
// /flowpipe/flows/<name>/versions/<version>/spec
// /flowpipe/flows/<name>/status
//

const rootPrefix = "/flowpipe/flows"

func flowPrefix(name string) string {
	return path.Join(rootPrefix, name)
}

func activeKey(name string) string {
	return path.Join(flowPrefix(name), "active")
}

func versionSpecKey(name string, version uint64) string {
	return path.Join(
		flowPrefix(name),
		"versions",
		strconv.FormatUint(version, 10),
		"spec",
	)
}

func statusKey(name string) string {
	return path.Join(flowPrefix(name), "status")
}

// ============================================================
// CreateFlow (version starts at 1)
// ============================================================

func (s *EtcdStore) CreateFlow(
	ctx context.Context,
	spec *flowpipev1.FlowSpec,
) (*flowpipev1.Flow, error) {

	if spec == nil || spec.Name == "" {
		return nil, fmt.Errorf("flow name is required")
	}

	return s.createNewVersion(ctx, spec.Name, spec)
}

// ============================================================
// UpdateFlow (creates new version)
// ============================================================

func (s *EtcdStore) UpdateFlow(
	ctx context.Context,
	name string,
	spec *flowpipev1.FlowSpec,
) (*flowpipev1.Flow, error) {

	if spec == nil {
		return nil, fmt.Errorf("spec is nil")
	}
	spec.Name = name

	return s.createNewVersion(ctx, name, spec)
}

// ============================================================
// Shared version-creation logic (CAS-safe)
// ============================================================

func (s *EtcdStore) createNewVersion(
	ctx context.Context,
	name string,
	spec *flowpipev1.FlowSpec,
) (*flowpipev1.Flow, error) {

	resp, err := s.cli.Get(ctx, activeKey(name))
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
			return nil, fmt.Errorf("invalid active version")
		}
		nextVersion = prevVersion + 1
	}

	spec.Version = nextVersion

	specBytes, err := proto.Marshal(spec)
	if err != nil {
		return nil, err
	}

	txn := s.cli.Txn(ctx)

	if len(resp.Kvs) == 0 {
		txn = txn.If(
			clientv3.Compare(
				clientv3.CreateRevision(activeKey(name)),
				"=",
				0,
			),
		)
	} else {
		txn = txn.If(
			clientv3.Compare(
				clientv3.Value(activeKey(name)),
				"=",
				string(resp.Kvs[0].Value),
			),
		)
	}

	txn = txn.Then(
		clientv3.OpPut(versionSpecKey(name, nextVersion), string(specBytes)),
		clientv3.OpPut(activeKey(name), strconv.FormatUint(nextVersion, 10)),
	)

	res, err := txn.Commit()
	if err != nil {
		return nil, err
	}
	if !res.Succeeded {
		return nil, fmt.Errorf("flow %q modified concurrently", name)
	}

	return &flowpipev1.Flow{
		Name:    name,
		Version: nextVersion,
		Spec:    spec,
		Status:  nil,
	}, nil
}

// ============================================================
// GetFlow
// ============================================================

func (s *EtcdStore) GetFlow(
	ctx context.Context,
	name string,
) (*flowpipev1.Flow, error) {

	activeResp, err := s.cli.Get(ctx, activeKey(name))
	if err != nil {
		return nil, err
	}
	if len(activeResp.Kvs) == 0 {
		return nil, fmt.Errorf("flow %q not found", name)
	}

	version, err := strconv.ParseUint(string(activeResp.Kvs[0].Value), 10, 64)
	if err != nil {
		return nil, fmt.Errorf("invalid active version")
	}

	specResp, err := s.cli.Get(ctx, versionSpecKey(name, version))
	if err != nil {
		return nil, err
	}
	if len(specResp.Kvs) == 0 {
		return nil, fmt.Errorf("spec missing for %q version %d", name, version)
	}

	var spec flowpipev1.FlowSpec
	if err := proto.Unmarshal(specResp.Kvs[0].Value, &spec); err != nil {
		return nil, err
	}

	status, _ := s.GetFlowStatus(ctx, name)

	return &flowpipev1.Flow{
		Name:    name,
		Version: version,
		Spec:    &spec,
		Status:  status,
	}, nil
}

// ============================================================
// ListFlows
// ============================================================

func (s *EtcdStore) ListFlows(
	ctx context.Context,
) ([]*flowpipev1.Flow, error) {

	resp, err := s.cli.Get(ctx, rootPrefix, clientv3.WithPrefix())
	if err != nil {
		return nil, err
	}

	seen := map[string]struct{}{}
	out := []*flowpipev1.Flow{}

	for _, kv := range resp.Kvs {
		key := string(kv.Key)
		if !strings.HasSuffix(key, "/active") {
			continue
		}

		name := strings.TrimSuffix(
			strings.TrimPrefix(key, rootPrefix+"/"),
			"/active",
		)

		if _, ok := seen[name]; ok {
			continue
		}
		seen[name] = struct{}{}

		f, err := s.GetFlow(ctx, name)
		if err == nil {
			out = append(out, f)
		}
	}

	return out, nil
}

// ============================================================
// DeleteFlow
// ============================================================

func (s *EtcdStore) DeleteFlow(
	ctx context.Context,
	name string,
) error {

	_, err := s.cli.Delete(ctx, flowPrefix(name), clientv3.WithPrefix())
	return err
}

// ============================================================
// GetFlowStatus (optional, controller-owned)
// ============================================================

func (s *EtcdStore) GetFlowStatus(
	ctx context.Context,
	name string,
) (*flowpipev1.FlowStatus, error) {

	resp, err := s.cli.Get(ctx, statusKey(name))
	if err != nil || len(resp.Kvs) == 0 {
		return nil, nil
	}

	var status flowpipev1.FlowStatus
	if err := proto.Unmarshal(resp.Kvs[0].Value, &status); err != nil {
		return nil, err
	}

	return &status, nil
}

// ============================================================
// RollbackFlow (sets active version)
// ============================================================

func (s *EtcdStore) RollbackFlow(
	ctx context.Context,
	name string,
	version uint64,
) (*flowpipev1.Flow, error) {

	specKey := versionSpecKey(name, version)

	specResp, err := s.cli.Get(ctx, specKey)
	if err != nil {
		return nil, err
	}
	if len(specResp.Kvs) == 0 {
		return nil, fmt.Errorf("version %d does not exist", version)
	}

	_, err = s.cli.Put(
		ctx,
		activeKey(name),
		strconv.FormatUint(version, 10),
	)
	if err != nil {
		return nil, err
	}

	return s.GetFlow(ctx, name)
}
