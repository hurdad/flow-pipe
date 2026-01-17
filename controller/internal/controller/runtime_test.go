package controller

import (
	"context"
	"testing"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	appsv1 "k8s.io/api/apps/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes/fake"
)

func TestEnsureRuntimeCreatesDeployment(t *testing.T) {
	client := fake.NewSimpleClientset()
	image := "runtime:latest"
	otelEndpoint := "collector:4317"
	spec := &flowpipev1.FlowSpec{
		Name: "noop-observability",
		Kubernetes: &flowpipev1.KubernetesSettings{
			Image: &image,
		},
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
		true,
		otelEndpoint,
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

func TestEnsureRuntimeCreatesDaemonSet(t *testing.T) {
	client := fake.NewSimpleClientset()
	image := "runtime:latest"
	otelEndpoint := "collector:4317"
	spec := &flowpipev1.FlowSpec{
		Name: "noop-daemon",
		Kubernetes: &flowpipev1.KubernetesSettings{
			Image: &image,
		},
		KubernetesOptions: &flowpipev1.KubernetesOptions{
			StreamingWorkloadKind: flowpipev1.StreamingWorkloadKind_STREAMING_WORKLOAD_KIND_DAEMONSET,
		},
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
		true,
		otelEndpoint,
	)
	if err != nil {
		t.Fatalf("ensureRuntime error: %v", err)
	}
	if workload != "noop-daemon-runtime" {
		t.Fatalf("expected daemonset workload name, got %q", workload)
	}

	if _, err := client.CoreV1().ConfigMaps("default").Get(context.Background(), "noop-daemon-config", metav1.GetOptions{}); err != nil {
		t.Fatalf("expected configmap: %v", err)
	}

	daemonSet, err := client.AppsV1().DaemonSets("default").Get(context.Background(), "noop-daemon-runtime", metav1.GetOptions{})
	if err != nil {
		t.Fatalf("expected daemonset: %v", err)
	}
	if got := daemonSet.Spec.Template.Spec.Containers[0].Image; got != image {
		t.Fatalf("expected runtime image, got %q", got)
	}
}

func TestEnsureRuntimeCreatesJob(t *testing.T) {
	client := fake.NewSimpleClientset()
	image := "custom:tag"
	otelEndpoint := "collector:4317"
	spec := &flowpipev1.FlowSpec{
		Name: "simple-pipeline-job",
		Kubernetes: &flowpipev1.KubernetesSettings{
			Image: &image,
		},
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
		true,
		otelEndpoint,
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

func TestEnsureRuntimeCreatesCronJob(t *testing.T) {
	client := fake.NewSimpleClientset()
	image := "custom:tag"
	otelEndpoint := "collector:4317"
	schedule := "*/5 * * * *"
	spec := &flowpipev1.FlowSpec{
		Name: "simple-pipeline-cron",
		Kubernetes: &flowpipev1.KubernetesSettings{
			Image: &image,
		},
		KubernetesOptions: &flowpipev1.KubernetesOptions{
			Cron: &flowpipev1.KubernetesCronOptions{
				Schedule: schedule,
			},
		},
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
		true,
		otelEndpoint,
	)
	if err != nil {
		t.Fatalf("ensureRuntime error: %v", err)
	}
	if workload != "simple-pipeline-cron" {
		t.Fatalf("expected cronjob workload name, got %q", workload)
	}

	if _, err := client.CoreV1().ConfigMaps("default").Get(context.Background(), "simple-pipeline-cron-config", metav1.GetOptions{}); err != nil {
		t.Fatalf("expected configmap: %v", err)
	}

	cronJob, err := client.BatchV1().CronJobs("default").Get(context.Background(), "simple-pipeline-cron", metav1.GetOptions{})
	if err != nil {
		t.Fatalf("expected cronjob: %v", err)
	}
	if got := cronJob.Spec.Schedule; got != schedule {
		t.Fatalf("expected schedule %q, got %q", schedule, got)
	}
	if got := cronJob.Spec.JobTemplate.Spec.Template.Spec.Containers[0].Image; got != image {
		t.Fatalf("expected cronjob image %q, got %q", image, got)
	}
	if cronJob.Spec.JobTemplate.Spec.Template.Spec.RestartPolicy != corev1.RestartPolicyNever {
		t.Fatalf("expected restart policy never, got %q", cronJob.Spec.JobTemplate.Spec.Template.Spec.RestartPolicy)
	}
}

func TestEnsureRuntimeAppliesKubernetesOptions(t *testing.T) {
	client := fake.NewSimpleClientset()
	image := "runtime:latest"
	serviceAccount := "flow-runner"
	runtimeClass := "kata"
	otelEndpoint := "collector:4317"
	spec := &flowpipev1.FlowSpec{
		Name: "noop-options",
		Kubernetes: &flowpipev1.KubernetesSettings{
			Image: &image,
		},
		KubernetesOptions: &flowpipev1.KubernetesOptions{
			PodLabels: map[string]string{
				"team": "edge",
			},
			PodAnnotations: map[string]string{
				"example.com/trace": "true",
			},
			ServiceAccountName: &serviceAccount,
			ImagePullSecrets:   []string{"regcred"},
			RuntimeClassName:   &runtimeClass,
		},
		Execution: &flowpipev1.Execution{
			Mode: flowpipev1.ExecutionMode_EXECUTION_MODE_STREAMING,
		},
	}

	_, err := ensureRuntime(
		context.Background(),
		client,
		"default",
		spec,
		corev1.PullIfNotPresent,
		false,
		otelEndpoint,
	)
	if err != nil {
		t.Fatalf("ensureRuntime error: %v", err)
	}

	deploy, err := client.AppsV1().Deployments("default").Get(context.Background(), "noop-options-runtime", metav1.GetOptions{})
	if err != nil {
		t.Fatalf("expected deployment: %v", err)
	}

	if got := deploy.Spec.Template.Labels["team"]; got != "edge" {
		t.Fatalf("expected pod label to be applied, got %q", got)
	}
	if got := deploy.Spec.Template.Annotations["example.com/trace"]; got != "true" {
		t.Fatalf("expected pod annotation to be applied, got %q", got)
	}
	if got := deploy.Spec.Template.Spec.ServiceAccountName; got != serviceAccount {
		t.Fatalf("expected service account %q, got %q", serviceAccount, got)
	}
	if len(deploy.Spec.Template.Spec.ImagePullSecrets) != 1 || deploy.Spec.Template.Spec.ImagePullSecrets[0].Name != "regcred" {
		t.Fatalf("expected image pull secret to be applied, got %v", deploy.Spec.Template.Spec.ImagePullSecrets)
	}
	if deploy.Spec.Template.Spec.RuntimeClassName == nil || *deploy.Spec.Template.Spec.RuntimeClassName != runtimeClass {
		t.Fatalf("expected runtime class %q, got %v", runtimeClass, deploy.Spec.Template.Spec.RuntimeClassName)
	}
	if got := deploy.Spec.Template.Labels[flowLabelKey]; got != "noop-options" {
		t.Fatalf("expected flow label to be preserved, got %q", got)
	}
	if got := deploy.Spec.Template.Annotations[runtimeConfigHashKey]; got == "" {
		t.Fatalf("expected config hash annotation to be preserved")
	}
}

func TestEnsureRuntimeUpdatesDeployment(t *testing.T) {
	client := fake.NewSimpleClientset()
	image := "updated:latest"
	otelEndpoint := "collector:4317"
	spec := &flowpipev1.FlowSpec{
		Name: "update-runtime",
		Kubernetes: &flowpipev1.KubernetesSettings{
			Image: &image,
		},
		Execution: &flowpipev1.Execution{
			Mode: flowpipev1.ExecutionMode_EXECUTION_MODE_STREAMING,
		},
	}

	configMap := &corev1.ConfigMap{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "update-runtime-config",
			Namespace: "default",
		},
		Data: map[string]string{
			runtimeConfigKey: "previous",
		},
	}
	if _, err := client.CoreV1().ConfigMaps("default").Create(context.Background(), configMap, metav1.CreateOptions{}); err != nil {
		t.Fatalf("create configmap: %v", err)
	}

	deployment := &appsv1.Deployment{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "update-runtime-runtime",
			Namespace: "default",
			Labels: map[string]string{
				flowLabelKey: "stale",
			},
		},
		Spec: appsv1.DeploymentSpec{
			Selector: &metav1.LabelSelector{
				MatchLabels: map[string]string{
					flowLabelKey: "stale",
				},
			},
			Template: corev1.PodTemplateSpec{
				ObjectMeta: metav1.ObjectMeta{
					Labels: map[string]string{
						flowLabelKey: "stale",
					},
				},
				Spec: corev1.PodSpec{
					Containers: []corev1.Container{
						{
							Name:  "runtime",
							Image: "old:tag",
						},
					},
				},
			},
		},
	}
	if _, err := client.AppsV1().Deployments("default").Create(context.Background(), deployment, metav1.CreateOptions{}); err != nil {
		t.Fatalf("create deployment: %v", err)
	}

	workload, err := ensureRuntime(
		context.Background(),
		client,
		"default",
		spec,
		corev1.PullIfNotPresent,
		true,
		otelEndpoint,
	)
	if err != nil {
		t.Fatalf("ensureRuntime error: %v", err)
	}
	if workload != "update-runtime-runtime" {
		t.Fatalf("expected deployment workload name, got %q", workload)
	}

	updatedConfig, err := client.CoreV1().ConfigMaps("default").Get(context.Background(), "update-runtime-config", metav1.GetOptions{})
	if err != nil {
		t.Fatalf("get configmap: %v", err)
	}
	if updatedConfig.Data[runtimeConfigKey] == "" || updatedConfig.Data[runtimeConfigKey] == "previous" {
		t.Fatalf("expected configmap data to be updated")
	}

	updatedDeployment, err := client.AppsV1().Deployments("default").Get(context.Background(), "update-runtime-runtime", metav1.GetOptions{})
	if err != nil {
		t.Fatalf("get deployment: %v", err)
	}
	if got := updatedDeployment.Spec.Template.Spec.Containers[0].Image; got != image {
		t.Fatalf("expected updated image %q, got %q", image, got)
	}
	if got := updatedDeployment.Labels[flowLabelKey]; got != "update-runtime" {
		t.Fatalf("expected updated label %q, got %q", "update-runtime", got)
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
