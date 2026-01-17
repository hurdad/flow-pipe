package server

import (
	"context"
	"fmt"
	"net"
	"time"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/metric"
	"go.opentelemetry.io/otel/trace"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
	"google.golang.org/grpc/reflection"

	"github.com/hurdad/flow-pipe/api/internal/config"
	"github.com/hurdad/flow-pipe/api/internal/observability"
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
	if err := ensureAPIKeyConfigured(cfg); err != nil {
		return nil, err
	}
	if err := ensureGRPCServerTLSConfigured(cfg); err != nil {
		return nil, err
	}

	meter := otel.Meter("github.com/hurdad/flow-pipe/api/grpc")
	requestCount, err := meter.Int64Counter(
		"flow_api_grpc_requests_total",
		metric.WithDescription("Number of gRPC requests received"),
	)
	if err != nil {
		return nil, fmt.Errorf("grpc metrics: %w", err)
	}

	latency, err := meter.Float64Histogram(
		"flow_api_grpc_latency_ms",
		metric.WithUnit("ms"),
		metric.WithDescription("Latency of gRPC requests"),
	)
	if err != nil {
		return nil, fmt.Errorf("grpc metrics histogram: %w", err)
	}

	tracer := observability.Tracer("flow-api/grpc")

	lis, err := net.Listen("tcp", cfg.GRPCAddr)
	if err != nil {
		return nil, fmt.Errorf("listen %s: %w", cfg.GRPCAddr, err)
	}

	serverOpts := []grpc.ServerOption{
		grpc.ChainUnaryInterceptor(
			apiKeyUnaryInterceptor(cfg),
			unaryTracingInterceptor(tracer, requestCount, latency),
		),
	}
	if cfg.GRPCTLSEnabled {
		creds, err := credentials.NewServerTLSFromFile(cfg.GRPCTLSCertFile, cfg.GRPCTLSKeyFile)
		if err != nil {
			return nil, fmt.Errorf("grpc tls config: %w", err)
		}
		serverOpts = append(serverOpts, grpc.Creds(creds))
	}
	grpcServer := grpc.NewServer(serverOpts...)
	flowServer := service.NewFlowServer(store)
	schemaRegistryServer := service.NewSchemaRegistryServer(store)
	flowpipev1.RegisterFlowServiceServer(grpcServer, flowServer)
	flowpipev1.RegisterSchemaRegistryServiceServer(grpcServer, schemaRegistryServer)

	reflection.Register(grpcServer)

	return &GRPCServer{
		addr:   cfg.GRPCAddr,
		server: grpcServer,
		lis:    lis,
	}, nil
}

func (g *GRPCServer) Run() error {
	return g.server.Serve(g.lis)
}

func (g *GRPCServer) Shutdown() {
	g.server.GracefulStop()
}

func unaryTracingInterceptor(tracer trace.Tracer, counter metric.Int64Counter, latency metric.Float64Histogram) grpc.UnaryServerInterceptor {
	return func(ctx context.Context, req interface{}, info *grpc.UnaryServerInfo, handler grpc.UnaryHandler) (interface{}, error) {
		start := time.Now()
		ctx, span := tracer.Start(ctx, info.FullMethod)
		defer span.End()

		resp, err := handler(ctx, req)

		attributes := []attribute.KeyValue{attribute.String("rpc.method", info.FullMethod)}
		duration := float64(time.Since(start).Milliseconds())

		counter.Add(ctx, 1, metric.WithAttributes(attributes...))
		latency.Record(ctx, duration, metric.WithAttributes(attributes...))
		span.SetAttributes(attributes...)
		if err != nil {
			span.RecordError(err)
			span.SetStatus(codes.Error, err.Error())
		}

		return resp, err
	}
}
