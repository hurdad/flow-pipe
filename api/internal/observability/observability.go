package observability

import (
	"context"
	"fmt"
	"log/slog"
	"os"
	"time"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/exporters/otlp/otlplog/otlploggrpc"
	"go.opentelemetry.io/otel/exporters/otlp/otlpmetric/otlpmetricgrpc"
	"go.opentelemetry.io/otel/exporters/otlp/otlptrace/otlptracegrpc"
	otellog "go.opentelemetry.io/otel/log"
	"go.opentelemetry.io/otel/log/global"
	"go.opentelemetry.io/otel/propagation"
	sdklog "go.opentelemetry.io/otel/sdk/log"
	sdkmetric "go.opentelemetry.io/otel/sdk/metric"
	"go.opentelemetry.io/otel/sdk/resource"
	sdktrace "go.opentelemetry.io/otel/sdk/trace"
	"go.opentelemetry.io/otel/semconv/v1.26.0"
	"go.opentelemetry.io/otel/trace"

	"github.com/hurdad/flow-pipe/api/internal/config"
)

// Logger wraps a slog.Logger and an OpenTelemetry logger to emit records to both stdout and OTLP.
type Logger struct {
	slog       *slog.Logger
	otel       otellog.Logger
	attributes []attribute.KeyValue
}

// Info writes an informational log entry.
func (l Logger) Info(ctx context.Context, msg string, attrs ...slog.Attr) {
	l.write(ctx, otellog.SeverityInfo, slog.LevelInfo, msg, attrs...)
}

// Error writes an error log entry.
func (l Logger) Error(ctx context.Context, msg string, attrs ...slog.Attr) {
	l.write(ctx, otellog.SeverityError, slog.LevelError, msg, attrs...)
}

func (l Logger) write(ctx context.Context, severity otellog.Severity, level slog.Level, msg string, attrs ...slog.Attr) {
	if l.slog != nil {
		l.slog.LogAttrs(ctx, level, msg, attrs...)
	}

	if l.otel == nil {
		return
	}

	rec := otellog.Record{}
	rec.SetTimestamp(time.Now())
	rec.SetSeverity(severity)
	rec.SetBody(otellog.StringValue(msg))
	rec.AddAttributes(l.attributes...)

	for _, a := range attrs {
		rec.AddAttributes(attribute.String(a.Key, fmt.Sprint(a.Value.Any())))
	}

	l.otel.Emit(ctx, rec)
}

// Setup configures OpenTelemetry exporters for traces, metrics, and logs using the provided configuration.
// It returns a shutdown function that should be called on process exit, and a logger that mirrors messages
// to stdout and the OTLP log exporter.
func Setup(ctx context.Context, cfg config.Config) (func(context.Context) error, Logger, error) {
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

	logExporter, err := otlploggrpc.New(ctx,
		otlploggrpc.WithEndpoint(cfg.OTLPEndpoint),
		otlploggrpc.WithInsecure(),
	)
	if err != nil {
		return nil, Logger{}, fmt.Errorf("log exporter: %w", err)
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

	loggerProvider := sdklog.NewLoggerProvider(
		sdklog.WithProcessor(sdklog.NewBatchProcessor(logExporter)),
		sdklog.WithResource(res),
	)
	global.SetLoggerProvider(loggerProvider)

	slogger := slog.New(slog.NewTextHandler(os.Stdout, nil))
	otelLogger := loggerProvider.Logger(cfg.ServiceName)

	lg := Logger{
		slog:       slogger,
		otel:       otelLogger,
		attributes: []attribute.KeyValue{semconv.ServiceName(cfg.ServiceName)},
	}

	shutdown := func(ctx context.Context) error {
		var shutdownErr error
		if err := tracerProvider.Shutdown(ctx); err != nil {
			shutdownErr = fmt.Errorf("trace shutdown: %w", err)
		}

		if err := meterProvider.Shutdown(ctx); err != nil && shutdownErr == nil {
			shutdownErr = fmt.Errorf("metric shutdown: %w", err)
		}

		if err := loggerProvider.Shutdown(ctx); err != nil && shutdownErr == nil {
			shutdownErr = fmt.Errorf("log shutdown: %w", err)
		}

		return shutdownErr
	}

	return shutdown, lg, nil
}

// Tracer returns a named tracer from the global provider.
func Tracer(name string) trace.Tracer {
	return otel.Tracer(name)
}
