package kube

import (
	"fmt"
	"os"
	"path/filepath"

	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
)

// Client wraps Kubernetes clients used by the controller
type Client struct {
	Clientset *kubernetes.Clientset
	Config    *rest.Config
}

// New creates a Kubernetes client.
// It automatically detects:
//   - in-cluster config (when running in Kubernetes)
//   - kubeconfig (when running locally)
func New() (*Client, error) {
	cfg, err := loadConfig()
	if err != nil {
		return nil, err
	}

	cs, err := kubernetes.NewForConfig(cfg)
	if err != nil {
		return nil, fmt.Errorf("create kubernetes clientset: %w", err)
	}

	return &Client{
		Clientset: cs,
		Config:    cfg,
	}, nil
}

// loadConfig selects in-cluster or local kubeconfig
func loadConfig() (*rest.Config, error) {
	// Try in-cluster config first
	cfg, err := rest.InClusterConfig()
	if err == nil {
		return cfg, nil
	}

	// Fall back to kubeconfig
	kubeconfig := kubeconfigPath()
	cfg, err = clientcmd.BuildConfigFromFlags("", kubeconfig)
	if err != nil {
		return nil, fmt.Errorf("load kubeconfig (%s): %w", kubeconfig, err)
	}

	return cfg, nil
}

// kubeconfigPath resolves kubeconfig location
func kubeconfigPath() string {
	if env := os.Getenv("KUBECONFIG"); env != "" {
		return env
	}

	home, err := os.UserHomeDir()
	if err != nil {
		return ""
	}

	return filepath.Join(home, ".kube", "config")
}
