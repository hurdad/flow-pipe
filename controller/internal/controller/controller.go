package controller

import (
	"context"
	"sync"

	"github.com/hurdad/flow-pipe/controller/internal/store"
)

// Controller is the core reconciliation engine.
// It is intentionally small and stateless.
type Controller struct {
	store store.Store
	queue WorkQueue
}

// New creates a new controller instance.
//
// store: desired-state store (etcd-backed, abstracted)
// queue: work queue for reconciliation keys
func New(store store.Store, queue WorkQueue) *Controller {
	return &Controller{
		store: store,
		queue: queue,
	}
}

// Run starts worker goroutines and blocks until context cancellation.
func (c *Controller) Run(ctx context.Context) error {
	const workers = 1 // keep minimal for now

	var wg sync.WaitGroup

	for i := 0; i < workers; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			c.worker(ctx)
		}()
	}

	// Block until shutdown signal
	<-ctx.Done()

	// Stop accepting new work
	c.queue.Shutdown()

	// Wait for workers to finish
	wg.Wait()

	return nil
}
