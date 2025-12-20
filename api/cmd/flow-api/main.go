package main

import (
	"context"
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/hurdad/flow-pipe/api/internal/config"
	"github.com/hurdad/flow-pipe/api/internal/server"
	"github.com/hurdad/flow-pipe/api/internal/store"
)

func main() {
	// --------------------------------------------------
	// Load configuration
	// --------------------------------------------------
	cfg := config.Load()

	// --------------------------------------------------
	// Create etcd store
	// --------------------------------------------------
	st, err := store.NewEtcd(cfg.EtcdEndpoints)
	if err != nil {
		log.Fatalf("failed to connect to etcd: %v", err)
	}
	defer st.Close()

	// --------------------------------------------------
	// Create API server (gRPC + HTTP)
	// --------------------------------------------------
	srv, err := server.New(cfg, st)
	if err != nil {
		log.Fatalf("failed to create server: %v", err)
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

	log.Println("flow-api starting")

	// --------------------------------------------------
	// Run server (blocks until shutdown)
	// --------------------------------------------------
	if err := srv.Run(ctx); err != nil {
		log.Fatalf("flow-api exited with error: %v", err)
	}

	log.Println("flow-api stopped")
}
