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
// Etcd-backed controller store
// ============================================================

type EtcdStore struct {
	cli *clientv3.Client
}

// Compile-time interface check
var _ Store = (*EtcdStore)(nil)

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
// List flows (initial controller sync)
// ============================================================

func (s *EtcdStore) ListFlows(ctx context.Context) ([]*flowpipev1.Flow, error) {
	resp, err := s.cli.Get(
		ctx,
		rootPrefix+"/",
		clientv3.WithPrefix(),
	)
	if err != nil {
		return nil, err
	}

	flows := map[string]*flowpipev1.Flow{}

	for _, kv := range resp.Kvs {
		key := string(kv.Key)

		// We only care about active pointers
		if !strings.HasSuffix(key, "/active") {
			continue
		}

		name := path.Base(path.Dir(key))

		version, err := strconv.ParseUint(string(kv.Value), 10, 64)
		if err != nil {
			continue
		}

		specResp, err := s.cli.Get(ctx, versionSpecKey(name, version))
		if err != nil || len(specResp.Kvs) == 0 {
			continue
		}

		var spec flowpipev1.FlowSpec
		if err := proto.Unmarshal(specResp.Kvs[0].Value, &spec); err != nil {
			continue
		}

		flows[name] = &flowpipev1.Flow{
			Name: name,
			Spec: &spec,
		}
	}

	out := make([]*flowpipev1.Flow, 0, len(flows))
	for _, f := range flows {
		out = append(out, f)
	}

	return out, nil
}

// ============================================================
// Watch flows (desired state)
// ============================================================

type etcdWatchStream struct {
	ch     chan WatchEvent
	cancel context.CancelFunc
}

func (w *etcdWatchStream) Events() <-chan WatchEvent {
	return w.ch
}

func (w *etcdWatchStream) Stop() {
	w.cancel()
}

func (s *EtcdStore) WatchFlows(ctx context.Context) WatchStream {
	ctx, cancel := context.WithCancel(ctx)

	out := make(chan WatchEvent, 16)

	watchCh := s.cli.Watch(
		ctx,
		rootPrefix+"/",
		clientv3.WithPrefix(),
	)

	go func() {
		defer close(out)

		for wr := range watchCh {
			for _, ev := range wr.Events {
				key := string(ev.Kv.Key)

				// Only react to changes in desired state
				if !strings.HasSuffix(key, "/active") {
					continue
				}

				name := path.Base(path.Dir(key))

				spec, _, err := s.GetActiveFlow(ctx, name)
				if err != nil {
					continue
				}

				out <- WatchEvent{
					Type: WatchUpdated,
					Flow: &flowpipev1.Flow{
						Name: name,
						Spec: spec,
					},
				}
			}
		}
	}()

	return &etcdWatchStream{
		ch:     out,
		cancel: cancel,
	}
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
