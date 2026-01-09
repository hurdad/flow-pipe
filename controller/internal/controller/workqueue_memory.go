package controller

import "k8s.io/client-go/util/workqueue"

// MemoryQueue is an in-memory rate-limited work queue.
// It implements the WorkQueue interface.
type MemoryQueue struct {
	q workqueue.RateLimitingInterface
}

// NewMemoryQueue creates a new in-memory work queue.
func NewMemoryQueue() *MemoryQueue {
	return &MemoryQueue{
		q: workqueue.NewRateLimitingQueue(
			workqueue.DefaultControllerRateLimiter(),
		),
	}
}

func (m *MemoryQueue) Add(key string) {
	m.q.Add(key)
}

func (m *MemoryQueue) Get() (string, bool) {
	item, shutdown := m.q.Get()
	if shutdown {
		return "", false
	}
	return item.(string), true
}

func (m *MemoryQueue) Forget(key string) {
	m.q.Forget(key)
	m.q.Done(key)
}

func (m *MemoryQueue) Retry(key string) {
	m.q.AddRateLimited(key)
	m.q.Done(key)
}

func (m *MemoryQueue) Shutdown() {
	m.q.ShutDown()
}
