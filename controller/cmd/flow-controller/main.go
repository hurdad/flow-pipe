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
	// Load config
	// --------------------------------------------------
	cfg := config.Load()

	// --------------------------------------------------
	// Connect to etcd (desired state store)
	// --------------------------------------------------
	etcdClient, err := store.NewEtcd(cfg.EtcdEndpoints)
	if err != nil {
		log.Fatalf("failed to connect to etcd: %v", err)
	}
	defer etcdClient.Close()

	// --------------------------------------------------
	// Create work queue
	// --------------------------------------------------
	queue := controller.NewMemoryQueue()

	// --------------------------------------------------
	// Create controller
	// --------------------------------------------------
	ctrl := controller.New(etcdClient, queue)

	// --------------------------------------------------
	// Context + signal handling
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
		log.Fatalf("controller exited with error: %v", err)
	}

	log.Println("flow-controller stopped")
}
