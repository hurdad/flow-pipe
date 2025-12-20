package controller

import (
	"context"
	"log"
	"time"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"google.golang.org/protobuf/types/known/timestamppb"
)

func (c *Controller) worker(ctx context.Context) {
	for {
		select {
		case <-ctx.Done():
			return
		default:
			name, ok := c.queue.Get()
			if !ok {
				// queue empty → idle
				continue
			}

			if err := c.reconcile(ctx, name); err != nil {
				log.Printf("reconcile failed for %s: %v", name, err)
				c.queue.Retry(name)
			} else {
				c.queue.Forget(name)
			}
		}
	}
}

func (c *Controller) reconcile(ctx context.Context, name string) error {
	log.Printf("reconcile flow: %s", name)

	spec, version, err := c.store.GetActiveFlow(ctx, name)
	if err != nil {
		return err
	}

	if spec == nil {
		// Flow deleted or not yet fully written
		log.Printf("flow not found: %s", name)
		return nil
	}

	// Minimal, correct status update
	status := &flowpipev1.FlowStatus{
		State:         flowpipev1.FlowState_FLOW_STATE_PENDING,
		Message:       "flow accepted by controller",
		ActiveVersion: version,
		LastUpdated:   timestamppb.New(time.Now()),
	}

	if err := c.store.UpdateStatus(ctx, name, status); err != nil {
		return err
	}

	log.Printf("status updated for %s (version=%d)", name, version)
	return nil
}
