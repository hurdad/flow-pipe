package controller

import (
	"context"
	"log/slog"
	"sync"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/metric"
	"go.opentelemetry.io/otel/metric/noop"
	"go.opentelemetry.io/otel/trace"

	"github.com/hurdad/flow-pipe/controller/internal/observability"
	"github.com/hurdad/flow-pipe/controller/internal/store"
	"k8s.io/client-go/kubernetes"
)

// Controller is the core reconciliation engine.
// It is intentionally small and stateless.
type Controller struct {
	store  store.Store
	queue  WorkQueue
	prefix string
	kube   kubernetes.Interface

	runtimeNamespace string

	logger observability.Logger
	tracer trace.Tracer

	reconcileSuccess metric.Int64Counter
	reconcileFailure metric.Int64Counter
	watchEvents      metric.Int64Counter
}

// New creates a new controller instance.
//
// store: desired-state store (etcd-backed, abstracted)
// queue: work queue for reconciliation keys
// prefix: etcd path prefix to monitor
func New(
	store store.Store,
	queue WorkQueue,
	prefix string,
	logger observability.Logger,
	kube kubernetes.Interface,
	runtimeNamespace string,
) *Controller {
	meter := otelMeter()

	reconcileSuccess, _ := meter.Int64Counter(
		"flow_controller_reconcile_success_total",
		metric.WithDescription("number of successful reconcile executions"),
	)
	reconcileFailure, _ := meter.Int64Counter(
		"flow_controller_reconcile_failure_total",
		metric.WithDescription("number of failed reconcile executions"),
	)
	watchEvents, _ := meter.Int64Counter(
		"flow_controller_watch_events_total",
		metric.WithDescription("number of watch events received"),
	)

	return &Controller{
		store:            store,
		queue:            queue,
		prefix:           prefix,
		kube:             kube,
		runtimeNamespace: runtimeNamespace,
		logger:           logger,
		tracer:           observability.Tracer("github.com/hurdad/flow-pipe/controller"),
		reconcileSuccess: reconcileSuccess,
		reconcileFailure: reconcileFailure,
		watchEvents:      watchEvents,
	}
}

// Run starts the controller event loop and blocks until context cancellation.
func (c *Controller) Run(ctx context.Context) error {
	const workers = 1 // keep minimal for now

	c.logger.Info(ctx, "controller starting")

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
		c.logger.Info(ctx, "watch started")
		c.runWatch(ctx)
		c.logger.Info(ctx, "watch stopped")
	}()

	// ------------------------------------------------------------
	// Start workers
	// ------------------------------------------------------------
	var wg sync.WaitGroup

	for i := 0; i < workers; i++ {
		wg.Add(1)
		go func(id int) {
			defer wg.Done()
			c.logger.Info(ctx, "worker started", slog.Int("id", id))
			c.worker(ctx)
			c.logger.Info(ctx, "worker stopped", slog.Int("id", id))
		}(i)
	}

	// ------------------------------------------------------------
	// Block until shutdown
	// ------------------------------------------------------------
	<-ctx.Done()
	c.logger.Info(ctx, "controller shutdown requested")

	// Stop accepting new work
	c.queue.Shutdown()

	// Wait for workers to drain
	wg.Wait()

	c.logger.Info(ctx, "controller stopped")
	return nil
}

func otelMeter() metric.Meter {
	provider := otel.GetMeterProvider()
	if provider == nil {
		return noop.NewMeterProvider().Meter("github.com/hurdad/flow-pipe/controller")
	}

	return provider.Meter("github.com/hurdad/flow-pipe/controller")
}
