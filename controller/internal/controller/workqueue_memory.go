package controller

import (
	"sync"
	"time"

	"k8s.io/client-go/util/workqueue"
)

// MemoryQueue is an in-memory rate-limited work queue.
// It implements the WorkQueue interface.
type MemoryQueue struct {
	q        workqueue.RateLimitingInterface
	mu       sync.Mutex
	enqueued map[string]time.Time
}

// NewMemoryQueue creates a new in-memory work queue.
func NewMemoryQueue() *MemoryQueue {
	return &MemoryQueue{
		q: workqueue.NewRateLimitingQueue(
			workqueue.DefaultControllerRateLimiter(),
		),
		enqueued: make(map[string]time.Time),
	}
}

func (m *MemoryQueue) Add(key string) {
	m.markEnqueued(key)
	m.q.Add(key)
}

func (m *MemoryQueue) Get() (string, time.Duration, bool) {
	item, shutdown := m.q.Get()
	if shutdown {
		return "", 0, false
	}
	key := item.(string)
	lag := m.dequeueLag(key)
	return key, lag, true
}

func (m *MemoryQueue) Forget(key string) {
	m.clearEnqueued(key)
	m.q.Forget(key)
	m.q.Done(key)
}

func (m *MemoryQueue) Retry(key string) {
	m.markEnqueued(key)
	m.q.AddRateLimited(key)
	m.q.Done(key)
}

func (m *MemoryQueue) Len() int {
	return m.q.Len()
}

func (m *MemoryQueue) Shutdown() {
	m.q.ShutDown()
}

func (m *MemoryQueue) markEnqueued(key string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.enqueued[key] = time.Now()
}

func (m *MemoryQueue) clearEnqueued(key string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	delete(m.enqueued, key)
}

func (m *MemoryQueue) dequeueLag(key string) time.Duration {
	m.mu.Lock()
	defer m.mu.Unlock()
	enqueuedAt, ok := m.enqueued[key]
	if !ok {
		return 0
	}
	delete(m.enqueued, key)
	return time.Since(enqueuedAt)
}
