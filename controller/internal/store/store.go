package store

import (
	"context"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	clientv3 "go.etcd.io/etcd/client/v3"
)

// Store defines the controller's view of desired state.
type Store interface {
	// WatchFlows watches for changes to flows (desired state).
	WatchFlows(ctx context.Context) clientv3.WatchChan

	// GetActiveFlow returns the active FlowSpec and its version.
	GetActiveFlow(
		ctx context.Context,
		name string,
	) (*flowpipev1.FlowSpec, uint64, error)

	// UpdateStatus updates the controller-owned flow status.
	UpdateStatus(
		ctx context.Context,
		name string,
		status *flowpipev1.FlowStatus,
	) error

	// Close releases underlying resources.
	Close() error
}
