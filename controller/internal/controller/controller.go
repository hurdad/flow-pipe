package controller

import (
	"context"
	"log"
	"sync"

	"github.com/hurdad/flow-pipe/controller/internal/store"
)

// Controller is the core reconciliation engine.
// It is intentionally small and stateless.
type Controller struct {
	store  store.Store
	queue  WorkQueue
	prefix string
}

// New creates a new controller instance.
//
// store: desired-state store (etcd-backed, abstracted)
// queue: work queue for reconciliation keys
// prefix: etcd path prefix to monitor
func New(store store.Store, queue WorkQueue, prefix string) *Controller {
	return &Controller{
		store:  store,
		queue:  queue,
		prefix: prefix,
	}
}

// Run starts the controller event loop and blocks until context cancellation.
func (c *Controller) Run(ctx context.Context) error {
	const workers = 1 // keep minimal for now

	log.Println("[controller] starting")

	// ------------------------------------------------------------
	// Seed existing flows
	// ------------------------------------------------------------
	if err := c.enqueueExisting(ctx); err != nil {
		return err
	}

	// ------------------------------------------------------------
	// Start watch loop (ONLY source of new work)
	// ------------------------------------------------------------
	go func() {
		log.Println("[watch] started")
		c.runWatch(ctx)
		log.Println("[watch] stopped")
	}()

	// ------------------------------------------------------------
	// Start workers
	// ------------------------------------------------------------
	var wg sync.WaitGroup

	for i := 0; i < workers; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			log.Printf("[worker %d] started", id)
			c.worker(ctx)
			log.Printf("[worker %d] stopped", id)
		}(i)
	}

	// ------------------------------------------------------------
	// Block until shutdown
	// ------------------------------------------------------------
	<-ctx.Done()
	log.Println("[controller] shutdown requested")

	// Stop accepting new work
	c.queue.Shutdown()

	// Wait for workers to drain
	wg.Wait()

	log.Println("[controller] stopped")
	return nil
}
