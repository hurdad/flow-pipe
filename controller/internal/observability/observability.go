package observability

import (
	"context"
	"fmt"
	"log/slog"
	"os"
	"strings"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/exporters/otlp/otlpmetric/otlpmetricgrpc"
	"go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracegrpc"
	metricnoop "go.opentelemetry.io/otel/metric/noop"
	"go.opentelemetry.io/otel/propagation"
	sdkmetric "go.opentelemetry.io/otel/sdk/metric"
	"go.opentelemetry.io/otel/sdk/resource"
	sdktrace "go.opentelemetry.io/otel/sdk/trace"
	"go.opentelemetry.io/otel/semconv/v1.26.0"
	"go.opentelemetry.io/otel/trace"
	"go.opentelemetry.io/otel/trace/noop"

	"github.com/hurdad/flow-pipe/controller/internal/config"
)

// Logger wraps a slog.Logger for structured logging to stdout.
type Logger struct {
	slog *slog.Logger
}

// Info writes an informational log entry.
func (l Logger) Info(ctx context.Context, msg string, attrs ...slog.Attr) {
	l.write(ctx, slog.LevelInfo, msg, attrs...)
}

// Warn writes a warning log entry.
func (l Logger) Warn(ctx context.Context, msg string, attrs ...slog.Attr) {
	l.write(ctx, slog.LevelWarn, msg, attrs...)
}

// Error writes an error log entry.
func (l Logger) Error(ctx context.Context, msg string, attrs ...slog.Attr) {
	l.write(ctx, slog.LevelError, msg, attrs...)
}

func (l Logger) write(ctx context.Context, level slog.Level, msg string, attrs ...slog.Attr) {
	if l.slog != nil {
		l.slog.LogAttrs(ctx, level, msg, attrs...)
	}
}

// Setup configures OpenTelemetry exporters for traces and metrics using the provided configuration.
// It returns a shutdown function that should be called on process exit, and a logger that emits messages
// to stdout.
func Setup(ctx context.Context, cfg config.Config) (func(context.Context) error, Logger, error) {
	levelVar := new(slog.LevelVar)
	levelVar.Set(parseLogLevel(cfg.LogLevel))
	slogger := slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{Level: levelVar}))
	lg := Logger{
		slog: slogger,
	}

	if !cfg.ObservabilityEnabled {
		otel.SetTracerProvider(noop.NewTracerProvider())
		otel.SetMeterProvider(metricnoop.NewMeterProvider())
		return func(context.Context) error { return nil }, lg, nil
	}

	res, err := resource.New(
		ctx,
		resource.WithFromEnv(),
		resource.WithProcess(),
		resource.WithTelemetrySDK(),
		resource.WithHost(),
		resource.WithAttributes(
			semconv.ServiceName(cfg.ServiceName),
		),
	)
	if err != nil {
		return nil, Logger{}, fmt.Errorf("resource: %w", err)
	}

	traceExporter, err := otlptracegrpc.New(ctx,
		otlptracegrpc.WithEndpoint(cfg.OTLPEndpoint),
		otlptracegrpc.WithInsecure(),
	)
	if err != nil {
		return nil, Logger{}, fmt.Errorf("trace exporter: %w", err)
	}

	metricExporter, err := otlpmetricgrpc.New(ctx,
		otlpmetricgrpc.WithEndpoint(cfg.OTLPEndpoint),
		otlpmetricgrpc.WithInsecure(),
	)
	if err != nil {
		return nil, Logger{}, fmt.Errorf("metric exporter: %w", err)
	}

	tracerProvider := sdktrace.NewTracerProvider(
		sdktrace.WithBatcher(traceExporter),
		sdktrace.WithResource(res),
	)
	otel.SetTracerProvider(tracerProvider)
	otel.SetTextMapPropagator(propagation.TraceContext{})

	meterProvider := sdkmetric.NewMeterProvider(
		sdkmetric.WithReader(sdkmetric.NewPeriodicReader(metricExporter)),
		sdkmetric.WithResource(res),
	)
	otel.SetMeterProvider(meterProvider)

	shutdown := func(ctx context.Context) error {
		var shutdownErr error
		if err := tracerProvider.Shutdown(ctx); err != nil {
			shutdownErr = fmt.Errorf("trace shutdown: %w", err)
		}

		if err := meterProvider.Shutdown(ctx); err != nil && shutdownErr == nil {
			shutdownErr = fmt.Errorf("metric shutdown: %w", err)
		}

		return shutdownErr
	}

	return shutdown, lg, nil
}

func parseLogLevel(level string) slog.Level {
	switch strings.ToLower(level) {
	case "debug":
		return slog.LevelDebug
	case "warn", "warning":
		return slog.LevelWarn
	case "error":
		return slog.LevelError
	default:
		return slog.LevelInfo
	}
}

// Tracer returns a named tracer from the global provider.
func Tracer(name string) trace.Tracer {
	return otel.Tracer(name)
}
