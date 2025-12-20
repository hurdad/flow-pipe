package server

import (
	"context"
	"fmt"
	"net/http"

	"github.com/grpc-ecosystem/grpc-gateway/v2/runtime"
	"google.golang.org/grpc"

	"github.com/hurdad/flow-pipe/api/internal/config"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

type HTTPServer struct {
	addr   string
	server *http.Server
}

func NewHTTPServer(cfg config.Config) (*HTTPServer, error) {
	mux := runtime.NewServeMux()

	ctx := context.Background()
	opts := []grpc.DialOption{
		grpc.WithInsecure(), // OK for in-cluster; TLS later
	}

	if err := flowpipev1.RegisterFlowServiceHandlerFromEndpoint(
		ctx,
		mux,
		cfg.GRPCAddr,
		opts,
	); err != nil {
		return nil, fmt.Errorf("register gateway: %w", err)
	}

	srv := &http.Server{
		Addr:    cfg.HTTPAddr,
		Handler: mux,
	}

	return &HTTPServer{
		addr:   cfg.HTTPAddr,
		server: srv,
	}, nil
}

func (h *HTTPServer) Run() error {
	return h.server.ListenAndServe()
}

func (h *HTTPServer) Shutdown() {
	_ = h.server.Shutdown(context.Background())
}
