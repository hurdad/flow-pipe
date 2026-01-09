package controller

import (
	"context"
	"testing"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes/fake"
)

func TestEnsureRuntimeCreatesDeployment(t *testing.T) {
	client := fake.NewSimpleClientset()
	image := "runtime:latest"
	spec := &flowpipev1.FlowSpec{
		Name:    "noop-observability",
		Runtime: flowpipev1.FlowRuntime_FLOW_RUNTIME_BUILTIN,
		Image:   &image,
		Execution: &flowpipev1.Execution{
			Mode: flowpipev1.ExecutionMode_EXECUTION_MODE_STREAMING,
		},
	}

	workload, err := ensureRuntime(
		context.Background(),
		client,
		"default",
		spec,
		corev1.PullIfNotPresent,
	)
	if err != nil {
		t.Fatalf("ensureRuntime error: %v", err)
	}
	if workload != "noop-observability-runtime" {
		t.Fatalf("expected deployment workload name, got %q", workload)
	}

	if _, err := client.CoreV1().ConfigMaps("default").Get(context.Background(), "noop-observability-config", metav1.GetOptions{}); err != nil {
		t.Fatalf("expected configmap: %v", err)
	}

	deploy, err := client.AppsV1().Deployments("default").Get(context.Background(), "noop-observability-runtime", metav1.GetOptions{})
	if err != nil {
		t.Fatalf("expected deployment: %v", err)
	}
	if got := deploy.Spec.Template.Spec.Containers[0].Image; got != image {
		t.Fatalf("expected runtime image, got %q", got)
	}
	if got := deploy.Spec.Template.Spec.Containers[0].Args; len(got) != 1 || got[0] != runtimeConfigPath {
		t.Fatalf("expected runtime config arg %q, got %v", runtimeConfigPath, got)
	}
}

func TestEnsureRuntimeCreatesJob(t *testing.T) {
	client := fake.NewSimpleClientset()
	image := "custom:tag"
	spec := &flowpipev1.FlowSpec{
		Name:    "simple-pipeline-job",
		Runtime: flowpipev1.FlowRuntime_FLOW_RUNTIME_CONTAINER,
		Image:   &image,
		Execution: &flowpipev1.Execution{
			Mode: flowpipev1.ExecutionMode_EXECUTION_MODE_JOB,
		},
	}

	workload, err := ensureRuntime(
		context.Background(),
		client,
		"default",
		spec,
		corev1.PullIfNotPresent,
	)
	if err != nil {
		t.Fatalf("ensureRuntime error: %v", err)
	}
	if workload != "simple-pipeline-job" {
		t.Fatalf("expected job workload name, got %q", workload)
	}

	if _, err := client.CoreV1().ConfigMaps("default").Get(context.Background(), "simple-pipeline-job-config", metav1.GetOptions{}); err != nil {
		t.Fatalf("expected configmap: %v", err)
	}

	job, err := client.BatchV1().Jobs("default").Get(context.Background(), "simple-pipeline-job", metav1.GetOptions{})
	if err != nil {
		t.Fatalf("expected job: %v", err)
	}
	if got := job.Spec.Template.Spec.Containers[0].Image; got != image {
		t.Fatalf("expected job image %q, got %q", image, got)
	}
	if job.Spec.Template.Spec.RestartPolicy != corev1.RestartPolicyNever {
		t.Fatalf("expected restart policy never, got %q", job.Spec.Template.Spec.RestartPolicy)
	}
}
