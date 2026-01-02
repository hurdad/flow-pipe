package main

import (
	"context"
	"log/slog"
	"os"
	"os/signal"
	"syscall"

	"github.com/hurdad/flow-pipe/api/internal/config"
	"github.com/hurdad/flow-pipe/api/internal/observability"
	"github.com/hurdad/flow-pipe/api/internal/server"
	"github.com/hurdad/flow-pipe/api/internal/store"
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
	// Create etcd store
	// --------------------------------------------------
	st, err := store.NewEtcd(cfg.EtcdEndpoints)
	if err != nil {
		logger.Error(context.Background(), "failed to connect to etcd", slog.Any("error", err))
		os.Exit(1)
	}
	defer st.Close()

	// --------------------------------------------------
	// Create API server (gRPC + HTTP)
	// --------------------------------------------------
	srv, err := server.New(cfg, st)
	if err != nil {
		logger.Error(context.Background(), "failed to create server", slog.Any("error", err))
		os.Exit(1)
	}

	// --------------------------------------------------
	// Context + signal handling
	// --------------------------------------------------
	ctx, cancel := signal.NotifyContext(
		context.Background(),
		os.Interrupt,
		syscall.SIGTERM,
	)
	defer cancel()

	logger.Info(ctx, "flow-api starting")

	// --------------------------------------------------
	// Run server (blocks until shutdown)
	// --------------------------------------------------
	if err := srv.Run(ctx); err != nil {
		logger.Error(ctx, "flow-api exited with error", slog.Any("error", err))
		os.Exit(1)
	}

	logger.Info(ctx, "flow-api stopped")
}
