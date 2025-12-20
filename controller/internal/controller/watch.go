package controller

import (
	"context"
	"log"

	"github.com/hurdad/flow-pipe/controller/internal/store"
)

// enqueueExisting seeds the work queue with all existing flows at startup.
func (c *Controller) enqueueExisting(ctx context.Context) error {
	flows, err := c.store.ListFlows(ctx)
	if err != nil {
		return err
	}

	for _, f := range flows {
		c.queue.Add(f.Name)
	}

	log.Printf("[watch] seeded %d existing flows", len(flows))
	return nil
}

// runWatch listens for store watch events and enqueues reconcile keys.
func (c *Controller) runWatch(ctx context.Context) {
	watch := c.store.WatchFlows(ctx)
	defer watch.Stop()

	for {
		select {
		case <-ctx.Done():
			return

		case ev, ok := <-watch.Events():
			if !ok {
				log.Printf("[watch] stream closed")
				return
			}

			if ev.Flow == nil {
				continue
			}

			log.Printf(
				"[watch] event=%s flow=%s",
				ev.Type,
				ev.Flow.Name,
			)

			switch ev.Type {
			case store.WatchAdded, store.WatchUpdated, store.WatchDeleted:
				c.queue.Add(ev.Flow.Name)
			}
		}
	}
}
