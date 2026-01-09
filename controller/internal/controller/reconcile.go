package controller

import (
	"context"
	"log/slog"
	"time"

	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/metric"
	"go.opentelemetry.io/otel/trace"

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
				time.Sleep(50 * time.Millisecond)
				continue
			}

			c.logger.Info(ctx, "reconcile requested", slog.String("flow", name))
			if err := c.reconcile(ctx, name); err != nil {
				c.logger.Error(ctx, "reconcile failed", slog.String("flow", name), slog.Any("error", err))
				c.reconcileFailure.Add(ctx, 1, metric.WithAttributes(attribute.String("flow.name", name)))
				c.queue.Retry(name)
			} else {
				c.reconcileSuccess.Add(ctx, 1, metric.WithAttributes(attribute.String("flow.name", name)))
				c.queue.Forget(name)
			}
		}
	}
}

func (c *Controller) reconcile(ctx context.Context, name string) error {
	ctx, span := c.tracer.Start(ctx,
		"controller.reconcile",
		trace.WithAttributes(attribute.String("flow.name", name)),
	)
	defer span.End()

	spec, version, err := c.store.GetActiveFlow(ctx, name)
	if err != nil {
		span.RecordError(err)
		span.SetStatus(codes.Error, err.Error())
		return err
	}

	if spec == nil {
		// Flow deleted or not yet fully written
		c.logger.Info(ctx, "flow not found or incomplete", slog.String("flow", name))
		span.SetAttributes(attribute.String("result", "missing"))
		return nil
	}

	workload, err := ensureRuntime(
		ctx,
		c.kube,
		c.runtimeNamespace,
		spec,
		pullPolicyFromSpec(spec),
	)
	if err != nil {
		span.RecordError(err)
		span.SetStatus(codes.Error, err.Error())
		return err
	}

	// Minimal, correct status update
	status := &flowpipev1.FlowStatus{
		State:         flowpipev1.FlowState_FLOW_STATE_DEPLOYING,
		Message:       "flow runtime created",
		ActiveVersion: version,
		Workload:      workload,
		LastUpdated:   timestamppb.New(time.Now()),
	}

	if err := c.store.UpdateStatus(ctx, name, status); err != nil {
		span.RecordError(err)
		span.SetStatus(codes.Error, err.Error())
		return err
	}

	span.SetAttributes(
		attribute.Int64("flow.version", int64(version)),
		attribute.String("status.state", status.State.String()),
	)
	span.SetStatus(codes.Ok, "status updated")
	c.logger.Info(ctx, "flow status updated", slog.String("flow", name), slog.Int64("version", int64(version)))
	return nil
}
