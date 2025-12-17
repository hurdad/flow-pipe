package controller

import (
	"context"
	"sync"

	clientv3 "go.etcd.io/etcd/client/v3"
)

// Controller is the core reconciliation engine.
// It is intentionally small and stateless.
type Controller struct {
	store *clientv3.Client
	queue WorkQueue
}

// New creates a new controller instance.
//
// store: persistent desired-state store (etcd)
// queue: work queue for reconciliation keys
func New(store *clientv3.Client, queue WorkQueue) *Controller {
	return &Controller{
		store: store,
		queue: queue,
	}
}

// Run starts worker goroutines and blocks until context cancellation.
func (c *Controller) Run(ctx context.Context) error {
	const workers = 4

	var wg sync.WaitGroup

	for i := 0; i < workers; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			c.worker(ctx)
		}(i)
	}

	// Block until shutdown signal
	<-ctx.Done()

	// Stop accepting new work
	c.queue.Shutdown()

	// Wait for workers to finish
	wg.Wait()

	return nil
}
