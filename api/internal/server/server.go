package server

import (
	"context"
	"fmt"

	"github.com/hurdad/flow-pipe/api/internal/config"
	"github.com/hurdad/flow-pipe/api/internal/store"
)

type Server struct {
	grpc *GRPCServer
	http *HTTPServer
}

func New(cfg config.Config, store store.Store) (*Server, error) {
	grpcSrv, err := NewGRPCServer(cfg, store)
	if err != nil {
		return nil, fmt.Errorf("grpc server: %w", err)
	}

	httpSrv, err := NewHTTPServer(cfg)
	if err != nil {
		return nil, fmt.Errorf("http server: %w", err)
	}

	return &Server{
		grpc: grpcSrv,
		http: httpSrv,
	}, nil
}

func (s *Server) Run(ctx context.Context) error {
	errCh := make(chan error, 2)

	go func() {
		errCh <- s.grpc.Run()
	}()

	go func() {
		errCh <- s.http.Run()
	}()

	select {
	case <-ctx.Done():
		s.grpc.Shutdown()
		s.http.Shutdown()
		return nil
	case err := <-errCh:
		s.grpc.Shutdown()
		s.http.Shutdown()
		return err
	}
}
