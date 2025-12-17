package controller

import (
	"context"
	"log"
)

func (c *Controller) worker(ctx context.Context) {
	for {
		key, ok := c.queue.Get()
		if !ok {
			return
		}

		if err := c.reconcile(ctx, key); err != nil {
			log.Printf("reconcile failed for %s: %v", key, err)
			c.queue.Retry(key)
		} else {
			c.queue.Forget(key)
		}
	}
}

func (c *Controller) reconcile(ctx context.Context, key string) error {
	// 1. Load desired state (Flow spec) from etcd
	// 2. Load actual state (Pods / Jobs) from Kubernetes
	// 3. Compute diff
	// 4. Apply changes (create/update/delete)
	return nil
}
