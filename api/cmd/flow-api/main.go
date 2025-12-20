package main

import (
	"context"
	"flag"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/hurdad/flow-pipe/api/flow"
	"github.com/hurdad/flow-pipe/api/store"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"

	"github.com/grpc-ecosystem/grpc-gateway/v2/runtime"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// --------------------------------------------------
// Config
// --------------------------------------------------

type config struct {
	grpcAddr string
	httpAddr string
	etcdURLs []string
}

// envOrDefault returns the environment variable value if set,
// otherwise returns the provided default.
func envOrDefault(key, def string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return def
}

func loadConfig() config {
	var (
		grpcAddr = flag.String(
			"grpc-addr",
			envOrDefault("FLOW_GRPC_ADDR", ":9090"),
			"gRPC listen address",
		)
		httpAddr = flag.String(
			"http-addr",
			envOrDefault("FLOW_HTTP_ADDR", ":8080"),
			"HTTP (grpc-gateway) listen address",
		)
		etcdURLs = flag.String(
			"etcd",
			envOrDefault("ETCD_ENDPOINTS", "http://127.0.0.1:2379"),
			"comma-separated etcd endpoints",
		)
	)

	flag.Parse()

	return config{
		grpcAddr: *grpcAddr,
		httpAddr: *httpAddr,
		etcdURLs: splitComma(*etcdURLs),
	}
}

// --------------------------------------------------
// Main
// --------------------------------------------------

func main() {
	cfg := loadConfig()

	// ---- etcd client ----
	etcdClient, err := clientv3.New(clientv3.Config{
		Endpoints:   cfg.etcdURLs,
		DialTimeout: 5 * time.Second,
	})
	if err != nil {
		log.Fatalf("failed to connect to etcd: %v", err)
	}
	defer etcdClient.Close()

	store := store.NewEtcdStore(etcdClient)
	flowServer := flow.NewFlowServer(store)

	// ---- gRPC server ----
	grpcLis, err := net.Listen("tcp", cfg.grpcAddr)
	if err != nil {
		log.Fatalf("failed to listen on %s: %v", cfg.grpcAddr, err)
	}

	grpcServer := grpc.NewServer()
	flowpipev1.RegisterFlowServiceServer(grpcServer, flowServer)

	// ---- HTTP gateway ----
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	gwmux := runtime.NewServeMux()
	err = flowpipev1.RegisterFlowServiceHandlerFromEndpoint(
		ctx,
		gwmux,
		cfg.grpcAddr,
		[]grpc.DialOption{grpc.WithTransportCredentials(insecure.NewCredentials())},
	)
	if err != nil {
		log.Fatalf("failed to register gateway: %v", err)
	}

	httpServer := &http.Server{
		Addr:    cfg.httpAddr,
		Handler: gwmux,
	}

	// ---- Run servers ----
	go func() {
		log.Printf("gRPC listening on %s", cfg.grpcAddr)
		if err := grpcServer.Serve(grpcLis); err != nil {
			log.Fatalf("gRPC server error: %v", err)
		}
	}()

	go func() {
		log.Printf("HTTP listening on %s", cfg.httpAddr)
		if err := httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("HTTP server error: %v", err)
		}
	}()

	// ---- Graceful shutdown ----
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)

	<-sigCh
	log.Println("shutting down")

	grpcServer.GracefulStop()
	_ = httpServer.Shutdown(context.Background())
}

// --------------------------------------------------
// Helpers
// --------------------------------------------------

func splitComma(s string) []string {
	parts := strings.Split(s, ",")
	out := make([]string, 0, len(parts))
	for _, p := range parts {
		if v := strings.TrimSpace(p); v != "" {
			out = append(out, v)
		}
	}
	return out
}
