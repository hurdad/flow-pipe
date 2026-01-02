package main

import (
	"context"
	"log/slog"
	"os"
	"os/signal"
	"syscall"

	"github.com/hurdad/flow-pipe/controller/internal/config"
	"github.com/hurdad/flow-pipe/controller/internal/controller"
	"github.com/hurdad/flow-pipe/controller/internal/observability"
	"github.com/hurdad/flow-pipe/controller/internal/store"
)

func main() {
	// --------------------------------------------------
	// Load configuration
	// --------------------------------------------------
	cfg := config.Load()

	// --------------------------------------------------
	// Initialize OpenTelemetry (traces/metrics/logs)
	// --------------------------------------------------
	shutdownTelemetry, logger, err := observability.Setup(context.Background(), cfg)
	if err != nil {
		panic(err)
	}
	defer func() {
		if err := shutdownTelemetry(context.Background()); err != nil {
			logger.Error(context.Background(), "failed to shutdown telemetry", slog.Any("error", err))
		}
	}()

	// --------------------------------------------------
	// Controller identity (used for HA / ownership)
	// --------------------------------------------------
	nodeName := os.Getenv("POD_NAME")
	if nodeName == "" {
		nodeName = "flow-controller-local-default"
	}

	// --------------------------------------------------
	// Create etcd-backed store (desired state)
	// --------------------------------------------------
	st, err := store.NewEtcd(cfg.EtcdEndpoints)
	if err != nil {
		logger.Error(context.Background(), "failed to connect to etcd", slog.Any("error", err))
		os.Exit(1)
	}
	defer st.Close()

	// --------------------------------------------------
	// Create work queue
	// --------------------------------------------------
	queue := controller.NewMemoryQueue()

	// --------------------------------------------------
	// Create controller
	// --------------------------------------------------
	ctrl := controller.New(st, queue, nodeName, logger)

	// --------------------------------------------------
	// Signal-aware context
	// --------------------------------------------------
	ctx, cancel := signal.NotifyContext(
		context.Background(),
		os.Interrupt,
		syscall.SIGTERM,
	)
	defer cancel()

	logger.Info(ctx, "flow-controller starting")

	// --------------------------------------------------
	// Run controller (blocks until shutdown)
	// --------------------------------------------------
	if err := ctrl.Run(ctx); err != nil {
		logger.Error(ctx, "flow-controller exited with error", slog.Any("error", err))
		os.Exit(1)
	}

	logger.Info(ctx, "flow-controller stopped")
}
