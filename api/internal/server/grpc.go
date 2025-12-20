package server

import (
	"fmt"
	"net"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"

	"github.com/hurdad/flow-pipe/api/internal/config"
	"github.com/hurdad/flow-pipe/api/internal/service"
	"github.com/hurdad/flow-pipe/api/internal/store"
	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
)

type GRPCServer struct {
	addr   string
	server *grpc.Server
	lis    net.Listener
}

func NewGRPCServer(cfg config.Config, store store.Store) (*GRPCServer, error) {
	lis, err := net.Listen("tcp", cfg.GRPCAddr)
	if err != nil {
		return nil, fmt.Errorf("listen %s: %w", cfg.GRPCAddr, err)
	}

	s := grpc.NewServer(
		// interceptors later (auth, otel, logging)
	)

	flowpipev1.RegisterFlowServiceServer(
		s,
		service.NewFlowService(store),
	)

	reflection.Register(s)

	return &GRPCServer{
		addr:   cfg.GRPCAddr,
		server: s,
		lis:    lis,
	}, nil
}

func (g *GRPCServer) Run() error {
	return g.server.Serve(g.lis)
}

func (g *GRPCServer) Shutdown() {
	g.server.GracefulStop()
}
