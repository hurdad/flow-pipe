package controller

import (
	"context"
	"log/slog"

	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/metric"

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

	c.logger.Info(ctx, "seeded existing flows", slog.Int("count", len(flows)))
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
				c.logger.Info(ctx, "watch stream closed")
				return
			}

			if ev.Flow == nil {
				continue
			}

			c.watchEvents.Add(ctx, 1, metric.WithAttributes(
				attribute.String("flow.name", ev.Flow.Name),
				attribute.String("event.type", string(ev.Type)),
			))
			c.logger.Info(
				ctx,
				"watch event received",
				slog.String("event", string(ev.Type)),
				slog.String("flow", ev.Flow.Name),
			)

			switch ev.Type {
			case store.WatchAdded, store.WatchUpdated, store.WatchDeleted:
				c.queue.Add(ev.Flow.Name)
			}
		}
	}
}
