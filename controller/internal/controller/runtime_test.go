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
		Name:  "noop-observability",
		Image: &image,
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
		Name:  "simple-pipeline-job",
		Image: &image,
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

func TestPullPolicyFromSpec(t *testing.T) {
	cases := []struct {
		name     string
		spec     *flowpipev1.FlowSpec
		expected corev1.PullPolicy
	}{
		{
			name:     "nil spec defaults",
			spec:     nil,
			expected: corev1.PullIfNotPresent,
		},
		{
			name: "always",
			spec: &flowpipev1.FlowSpec{
				Kubernetes: &flowpipev1.KubernetesSettings{
					ImagePullPolicy: flowpipev1.ImagePullPolicy_IMAGE_PULL_POLICY_ALWAYS,
				},
			},
			expected: corev1.PullAlways,
		},
		{
			name: "never",
			spec: &flowpipev1.FlowSpec{
				Kubernetes: &flowpipev1.KubernetesSettings{
					ImagePullPolicy: flowpipev1.ImagePullPolicy_IMAGE_PULL_POLICY_NEVER,
				},
			},
			expected: corev1.PullNever,
		},
		{
			name: "if not present",
			spec: &flowpipev1.FlowSpec{
				Kubernetes: &flowpipev1.KubernetesSettings{
					ImagePullPolicy: flowpipev1.ImagePullPolicy_IMAGE_PULL_POLICY_IF_NOT_PRESENT,
				},
			},
			expected: corev1.PullIfNotPresent,
		},
		{
			name: "unspecified",
			spec: &flowpipev1.FlowSpec{
				Kubernetes: &flowpipev1.KubernetesSettings{
					ImagePullPolicy: flowpipev1.ImagePullPolicy_IMAGE_PULL_POLICY_UNSPECIFIED,
				},
			},
			expected: corev1.PullIfNotPresent,
		},
		{
			name:     "nil kubernetes",
			spec:     &flowpipev1.FlowSpec{},
			expected: corev1.PullIfNotPresent,
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := pullPolicyFromSpec(tc.spec); got != tc.expected {
				t.Fatalf("expected %q, got %q", tc.expected, got)
			}
		})
	}
}

func TestRestartPolicyFromSpec(t *testing.T) {
	cases := []struct {
		name     string
		spec     *flowpipev1.FlowSpec
		expected corev1.RestartPolicy
	}{
		{
			name:     "nil spec defaults",
			spec:     nil,
			expected: corev1.RestartPolicyNever,
		},
		{
			name: "always",
			spec: &flowpipev1.FlowSpec{
				Kubernetes: &flowpipev1.KubernetesSettings{
					RestartPolicy: flowpipev1.RestartPolicy_RESTART_POLICY_ALWAYS,
				},
			},
			expected: corev1.RestartPolicyAlways,
		},
		{
			name: "on failure",
			spec: &flowpipev1.FlowSpec{
				Kubernetes: &flowpipev1.KubernetesSettings{
					RestartPolicy: flowpipev1.RestartPolicy_RESTART_POLICY_ON_FAILURE,
				},
			},
			expected: corev1.RestartPolicyOnFailure,
		},
		{
			name: "never",
			spec: &flowpipev1.FlowSpec{
				Kubernetes: &flowpipev1.KubernetesSettings{
					RestartPolicy: flowpipev1.RestartPolicy_RESTART_POLICY_NEVER,
				},
			},
			expected: corev1.RestartPolicyNever,
		},
		{
			name: "unspecified",
			spec: &flowpipev1.FlowSpec{
				Kubernetes: &flowpipev1.KubernetesSettings{
					RestartPolicy: flowpipev1.RestartPolicy_RESTART_POLICY_UNSPECIFIED,
				},
			},
			expected: corev1.RestartPolicyNever,
		},
		{
			name:     "nil kubernetes",
			spec:     &flowpipev1.FlowSpec{},
			expected: corev1.RestartPolicyNever,
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := restartPolicyFromSpec(tc.spec); got != tc.expected {
				t.Fatalf("expected %q, got %q", tc.expected, got)
			}
		})
	}
}
