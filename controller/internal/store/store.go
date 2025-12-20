package store

import (
	"context"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

//
// Watch types (store-level, not etcd-level)
//

type WatchEventType string

const (
	WatchAdded   WatchEventType = "ADDED"
	WatchUpdated WatchEventType = "UPDATED"
	WatchDeleted WatchEventType = "DELETED"
)

type WatchEvent struct {
	Type WatchEventType
	Flow *flowpipev1.Flow
}

// WatchStream is a long-lived stream of flow events.
type WatchStream interface {
	Events() <-chan WatchEvent
	Stop()
}

//
// Store defines the controller's view of desired state.
//

type Store interface {
	// ListFlows returns all flows (used for initial controller sync).
	ListFlows(ctx context.Context) ([]*flowpipev1.Flow, error)

	// WatchFlows watches for changes to flows (desired state).
	WatchFlows(ctx context.Context) WatchStream

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
