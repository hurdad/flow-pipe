package store

import (
	"context"
	"fmt"
	"path"
	"strconv"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/protobuf/proto"
)

// ============================================================
// Etcd-backed controller store
// ============================================================

type EtcdStore struct {
	cli *clientv3.Client
}

// NewEtcd creates a new etcd-backed store for the controller.
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
// Key layout (shared with API)
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
// Watch flows (desired state)
// ============================================================

func (s *EtcdStore) WatchFlows(ctx context.Context) clientv3.WatchChan {
	return s.cli.Watch(
		ctx,
		rootPrefix+"/",
		clientv3.WithPrefix(),
	)
}

// ============================================================
// Get active flow spec
// ============================================================

func (s *EtcdStore) GetActiveFlow(
	ctx context.Context,
	name string,
) (*flowpipev1.FlowSpec, uint64, error) {

	activeResp, err := s.cli.Get(ctx, activeKey(name))
	if err != nil {
		return nil, 0, err
	}
	if len(activeResp.Kvs) == 0 {
		return nil, 0, fmt.Errorf("flow %q not found", name)
	}

	version, err := strconv.ParseUint(string(activeResp.Kvs[0].Value), 10, 64)
	if err != nil {
		return nil, 0, fmt.Errorf("invalid active version")
	}

	specResp, err := s.cli.Get(ctx, versionSpecKey(name, version))
	if err != nil {
		return nil, 0, err
	}
	if len(specResp.Kvs) == 0 {
		return nil, 0, fmt.Errorf("spec missing for %q v%d", name, version)
	}

	var spec flowpipev1.FlowSpec
	if err := proto.Unmarshal(specResp.Kvs[0].Value, &spec); err != nil {
		return nil, 0, err
	}

	return &spec, version, nil
}

// ============================================================
// Update flow status (controller-owned)
// ============================================================

func (s *EtcdStore) UpdateStatus(
	ctx context.Context,
	name string,
	status *flowpipev1.FlowStatus,
) error {

	if status == nil {
		return fmt.Errorf("status is nil")
	}

	data, err := proto.Marshal(status)
	if err != nil {
		return err
	}

	_, err = s.cli.Put(ctx, statusKey(name), string(data))
	return err
}
