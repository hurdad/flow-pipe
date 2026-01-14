package main

import (
	"context"
	"log/slog"
	"os"
	"os/signal"
	"syscall"

	"github.com/hurdad/flow-pipe/controller/internal/config"
	"github.com/hurdad/flow-pipe/controller/internal/controller"
	"github.com/hurdad/flow-pipe/controller/internal/kube"
	"github.com/hurdad/flow-pipe/controller/internal/observability"
	"github.com/hurdad/flow-pipe/controller/internal/store"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/tools/leaderelection"
	"k8s.io/client-go/tools/leaderelection/resourcelock"
)

func main() {
	// --------------------------------------------------
	// Load configuration
	// --------------------------------------------------
	cfg := config.Load()

	// --------------------------------------------------
	// Initialize OpenTelemetry (traces/metrics/logs)
	// --------------------------------------------------
	shutdownTelemetry, logger, err := observability.Setup(context.Background(), cfg)
	if err != nil {
		panic(err)
	}
	defer func() {
		if err := shutdownTelemetry(context.Background()); err != nil {
			logger.Error(context.Background(), "failed to shutdown telemetry", slog.Any("error", err))
		}
	}()

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
		logger.Error(context.Background(), "failed to connect to etcd", slog.Any("error", err))
		os.Exit(1)
	}
	defer st.Close()

	// --------------------------------------------------
	// Create work queue
	// --------------------------------------------------
	queue := controller.NewMemoryQueue()

	// --------------------------------------------------
	// Create Kubernetes client
	// --------------------------------------------------
	kubeClient, err := kube.New()
	if err != nil {
		logger.Error(context.Background(), "failed to create kubernetes client", slog.Any("error", err))
		os.Exit(1)
	}

	// --------------------------------------------------
	// Create controller
	// --------------------------------------------------
	ctrl := controller.New(
		st,
		queue,
		nodeName,
		logger,
		kubeClient.Clientset,
		cfg.RuntimeNamespace,
		cfg.ObservabilityEnabled,
		cfg.WorkerCount,
	)

	// --------------------------------------------------
	// Signal-aware context
	// --------------------------------------------------
	ctx, cancel := signal.NotifyContext(
		context.Background(),
		os.Interrupt,
		syscall.SIGTERM,
	)
	defer cancel()

	logger.Info(ctx, "flow-controller starting")

	// --------------------------------------------------
	// Run controller (blocks until shutdown)
	// --------------------------------------------------
	if cfg.LeaderElectionEnabled {
		if err := runWithLeaderElection(ctx, cfg, nodeName, kubeClient, logger, ctrl); err != nil {
			logger.Error(ctx, "flow-controller exited with error", slog.Any("error", err))
			os.Exit(1)
		}
	} else {
		if err := ctrl.Run(ctx); err != nil {
			logger.Error(ctx, "flow-controller exited with error", slog.Any("error", err))
			os.Exit(1)
		}
	}

	logger.Info(ctx, "flow-controller stopped")
}

func runWithLeaderElection(
	ctx context.Context,
	cfg config.Config,
	nodeName string,
	kubeClient *kube.Client,
	logger observability.Logger,
	ctrl *controller.Controller,
) error {
	lock := &resourcelock.LeaseLock{
		LeaseMeta: metav1.ObjectMeta{
			Name:      cfg.LeaderElectionName,
			Namespace: cfg.LeaderElectionNamespace,
		},
		Client: kubeClient.Clientset.CoordinationV1(),
		LockConfig: resourcelock.ResourceLockConfig{
			Identity: nodeName,
		},
	}

	errCh := make(chan error, 1)
	stoppedCh := make(chan struct{}, 1)

	leaderConfig := leaderelection.LeaderElectionConfig{
		Lock:            lock,
		LeaseDuration:   cfg.LeaderElectionLeaseDuration,
		RenewDeadline:   cfg.LeaderElectionRenewDeadline,
		RetryPeriod:     cfg.LeaderElectionRetryPeriod,
		ReleaseOnCancel: true,
		Callbacks: leaderelection.LeaderCallbacks{
			OnStartedLeading: func(leaderCtx context.Context) {
				logger.Info(leaderCtx, "leader election acquired", slog.String("identity", nodeName))
				if err := ctrl.Run(leaderCtx); err != nil {
					select {
					case errCh <- err:
					default:
					}
				}
			},
			OnStoppedLeading: func() {
				logger.Warn(ctx, "leader election lost", slog.String("identity", nodeName))
				select {
				case stoppedCh <- struct{}{}:
				default:
				}
			},
			OnNewLeader: func(identity string) {
				if identity == nodeName {
					return
				}
				logger.Info(ctx, "leader election new leader observed", slog.String("identity", identity))
			},
		},
	}

	leaderElector, err := leaderelection.NewLeaderElector(leaderConfig)
	if err != nil {
		return err
	}

	for {
		runDone := make(chan struct{})
		go func() {
			leaderElector.Run(ctx)
			close(runDone)
		}()

		select {
		case <-ctx.Done():
			return nil
		case err := <-errCh:
			return err
		case <-stoppedCh:
			<-runDone
			if ctx.Err() != nil {
				return nil
			}
			logger.Info(ctx, "restarting leader election", slog.String("identity", nodeName))
		}
	}
}
