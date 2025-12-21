package main

import (
	"context"
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/hurdad/flow-pipe/controller/internal/config"
	"github.com/hurdad/flow-pipe/controller/internal/controller"
	"github.com/hurdad/flow-pipe/controller/internal/store"
)

func main() {
	// --------------------------------------------------
	// Load configuration
	// --------------------------------------------------
	cfg := config.Load()

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
		log.Fatalf("failed to connect to etcd: %v", err)
	}
	defer st.Close()

	// --------------------------------------------------
	// Create work queue
	// --------------------------------------------------
	queue := controller.NewMemoryQueue()

	// --------------------------------------------------
	// Create controller
	// --------------------------------------------------
	ctrl := controller.New(st, queue, nodeName)

	// --------------------------------------------------
	// Signal-aware context
	// --------------------------------------------------
	ctx, cancel := signal.NotifyContext(
		context.Background(),
		os.Interrupt,
		syscall.SIGTERM,
	)
	defer cancel()

	log.Println("flow-controller starting")

	// --------------------------------------------------
	// Run controller (blocks until shutdown)
	// --------------------------------------------------
	if err := ctrl.Run(ctx); err != nil {
		log.Fatalf("flow-controller exited with error: %v", err)
	}

	log.Println("flow-controller stopped")
}
