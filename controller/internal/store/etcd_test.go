package store

import (
	"context"
	"testing"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"go.etcd.io/etcd/api/v3/mvccpb"
	clientv3 "go.etcd.io/etcd/client/v3"
)

func TestBuildWatchEvent_AddAndUpdate(t *testing.T) {
	tests := []struct {
		name      string
		event     *clientv3.Event
		wantType  WatchEventType
		wantSpec  bool
		wantFetch bool
	}{
		{
			name: "new flow",
			event: &clientv3.Event{
				Type: clientv3.EventTypePut,
				Kv: &mvccpb.KeyValue{
					Key:            []byte("/flowpipe/flows/example/active"),
					CreateRevision: 1,
					ModRevision:    1,
				},
			},
			wantType:  WatchAdded,
			wantSpec:  true,
			wantFetch: true,
		},
		{
			name: "updated flow",
			event: &clientv3.Event{
				Type: clientv3.EventTypePut,
				Kv: &mvccpb.KeyValue{
					Key:            []byte("/flowpipe/flows/example/active"),
					CreateRevision: 1,
					ModRevision:    2,
				},
			},
			wantType:  WatchUpdated,
			wantSpec:  true,
			wantFetch: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			fetched := false
			fetchActive := func(ctx context.Context, name string) (*flowpipev1.FlowSpec, uint64, error) {
				fetched = true
				if name != "example" {
					t.Fatalf("expected name example, got %s", name)
				}
				return &flowpipev1.FlowSpec{}, 1, nil
			}

			ev, ok := buildWatchEvent(context.Background(), tt.event, fetchActive)
			if !ok {
				t.Fatalf("expected event to be processed")
			}

			if ev.Type != tt.wantType {
				t.Fatalf("event type = %s, want %s", ev.Type, tt.wantType)
			}

			if (ev.Flow.Spec != nil) != tt.wantSpec {
				t.Fatalf("spec presence = %t, want %t", ev.Flow.Spec != nil, tt.wantSpec)
			}

			if fetched != tt.wantFetch {
				t.Fatalf("fetchActive called = %t, want %t", fetched, tt.wantFetch)
			}
		})
	}
}

func TestBuildWatchEvent_Delete(t *testing.T) {
	event := &clientv3.Event{
		Type: clientv3.EventTypeDelete,
		Kv: &mvccpb.KeyValue{
			Key: []byte("/flowpipe/flows/example/active"),
		},
	}

	fetchActive := func(ctx context.Context, name string) (*flowpipev1.FlowSpec, uint64, error) {
		t.Fatalf("fetchActive should not be called for delete events")
		return nil, 0, nil
	}

	ev, ok := buildWatchEvent(context.Background(), event, fetchActive)
	if !ok {
		t.Fatalf("expected delete event to be processed")
	}

	if ev.Type != WatchDeleted {
		t.Fatalf("event type = %s, want %s", ev.Type, WatchDeleted)
	}

	if ev.Flow == nil || ev.Flow.Name != "example" {
		t.Fatalf("unexpected flow payload %#v", ev.Flow)
	}

	if ev.Flow.Spec != nil {
		t.Fatalf("spec should be nil for delete events")
	}
}

func TestBuildWatchEvent_IgnoresNonActiveKeys(t *testing.T) {
	event := &clientv3.Event{
		Type: clientv3.EventTypePut,
		Kv: &mvccpb.KeyValue{
			Key: []byte("/flowpipe/flows/example/status"),
		},
	}

	fetchActive := func(ctx context.Context, name string) (*flowpipev1.FlowSpec, uint64, error) {
		t.Fatalf("fetchActive should not be called for ignored keys")
		return nil, 0, nil
	}

	if _, ok := buildWatchEvent(context.Background(), event, fetchActive); ok {
		t.Fatalf("expected event to be ignored")
	}
}
