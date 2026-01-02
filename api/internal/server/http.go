package server

import (
	"context"
	"fmt"
	"net/http"
	"time"

	"github.com/grpc-ecosystem/grpc-gateway/v2/runtime"
	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/metric"
	"go.opentelemetry.io/otel/trace"
	"google.golang.org/grpc"

	"github.com/hurdad/flow-pipe/api/internal/config"
	"github.com/hurdad/flow-pipe/api/internal/observability"
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

	meter := otel.Meter("github.com/hurdad/flow-pipe/api/http")
	requestCount, err := meter.Int64Counter(
		"flow_api_http_requests_total",
		metric.WithDescription("Number of HTTP gateway requests"),
	)
	if err != nil {
		return nil, fmt.Errorf("http metrics: %w", err)
	}

	latency, err := meter.Float64Histogram(
		"flow_api_http_latency_ms",
		metric.WithUnit("ms"),
		metric.WithDescription("Latency of HTTP gateway requests"),
	)
	if err != nil {
		return nil, fmt.Errorf("http latency metrics: %w", err)
	}

	tracer := observability.Tracer("flow-api/http")

	srv := &http.Server{
		Addr:    cfg.HTTPAddr,
		Handler: httpMiddleware(mux, tracer, requestCount, latency),
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

func httpMiddleware(next http.Handler, tracer trace.Tracer, counter metric.Int64Counter, latency metric.Float64Histogram) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		ctx, span := tracer.Start(r.Context(), r.Method+" "+r.URL.Path)
		start := time.Now()

		recorder := &statusRecorder{ResponseWriter: w, status: http.StatusOK}
		next.ServeHTTP(recorder, r.WithContext(ctx))

		attributes := []attribute.KeyValue{
			attribute.String("http.method", r.Method),
			attribute.String("http.route", r.URL.Path),
			attribute.Int("http.status_code", recorder.status),
		}

		duration := float64(time.Since(start).Milliseconds())
		counter.Add(ctx, 1, attributes...)
		latency.Record(ctx, duration, attributes...)
		span.SetAttributes(attributes...)
		if recorder.status >= http.StatusInternalServerError {
			span.SetStatus(codes.Error, http.StatusText(recorder.status))
		}
		span.End()
	})
}

type statusRecorder struct {
	http.ResponseWriter
	status int
}

func (r *statusRecorder) WriteHeader(statusCode int) {
	r.status = statusCode
	r.ResponseWriter.WriteHeader(statusCode)
}
