package controller

import (
	"context"
	"fmt"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"google.golang.org/protobuf/encoding/protojson"
	appsv1 "k8s.io/api/apps/v1"
	batchv1 "k8s.io/api/batch/v1"
	corev1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"sigs.k8s.io/yaml"
)

const (
	flowLabelKey          = "flowpipe.io/flow-name"
	runtimeConfigKey      = "flow.yaml"
	runtimeConfigMountDir = "/config"
	runtimeConfigPath     = "/config/flow.yaml"
)

func ensureRuntime(
	ctx context.Context,
	client kubernetes.Interface,
	namespace string,
	spec *flowpipev1.FlowSpec,
	imagePullPolicy corev1.PullPolicy,
) (string, error) {
	if client == nil {
		return "", fmt.Errorf("kubernetes client is required for runtime reconciliation")
	}
	if spec == nil {
		return "", fmt.Errorf("flow spec is required")
	}
	if spec.Name == "" {
		return "", fmt.Errorf("flow name is required")
	}

	image := ""
	if spec.Image != nil {
		image = *spec.Image
	}
	if image == "" {
		return "", fmt.Errorf("runtime image is required for flow %q", spec.Name)
	}

	configMapName := fmt.Sprintf("%s-config", spec.Name)
	if err := applyConfigMap(ctx, client, namespace, configMapName, spec); err != nil {
		return "", err
	}

	mode := flowpipev1.ExecutionMode_EXECUTION_MODE_STREAMING
	if spec.Execution != nil {
		mode = spec.Execution.Mode
	}

	switch mode {
	case flowpipev1.ExecutionMode_EXECUTION_MODE_JOB:
		return applyJob(ctx, client, namespace, spec.Name, configMapName, image, imagePullPolicy)
	case flowpipev1.ExecutionMode_EXECUTION_MODE_STREAMING:
		fallthrough
	default:
		deploymentName := fmt.Sprintf("%s-runtime", spec.Name)
		return applyDeployment(ctx, client, namespace, spec.Name, deploymentName, configMapName, image, imagePullPolicy)
	}
}

func applyConfigMap(
	ctx context.Context,
	client kubernetes.Interface,
	namespace string,
	name string,
	spec *flowpipev1.FlowSpec,
) error {
	payload, err := protojson.MarshalOptions{UseProtoNames: true}.Marshal(spec)
	if err != nil {
		return fmt.Errorf("marshal flow spec: %w", err)
	}
	yamlPayload, err := yaml.JSONToYAML(payload)
	if err != nil {
		return fmt.Errorf("marshal flow spec as yaml: %w", err)
	}

	desired := &corev1.ConfigMap{
		ObjectMeta: metav1.ObjectMeta{
			Name:      name,
			Namespace: namespace,
			Labels: map[string]string{
				flowLabelKey: spec.Name,
			},
		},
		Data: map[string]string{
			runtimeConfigKey: string(yamlPayload),
		},
	}

	cmClient := client.CoreV1().ConfigMaps(namespace)
	current, err := cmClient.Get(ctx, name, metav1.GetOptions{})
	if err != nil {
		if errors.IsNotFound(err) {
			_, err = cmClient.Create(ctx, desired, metav1.CreateOptions{})
			return err
		}
		return err
	}

	current.Data = desired.Data
	current.Labels = desired.Labels
	_, err = cmClient.Update(ctx, current, metav1.UpdateOptions{})
	return err
}

func applyDeployment(
	ctx context.Context,
	client kubernetes.Interface,
	namespace string,
	flowName string,
	name string,
	configMapName string,
	image string,
	imagePullPolicy corev1.PullPolicy,
) (string, error) {
	desired := &appsv1.Deployment{
		ObjectMeta: metav1.ObjectMeta{
			Name:      name,
			Namespace: namespace,
			Labels: map[string]string{
				flowLabelKey: flowName,
			},
		},
		Spec: appsv1.DeploymentSpec{
			Replicas: int32Ptr(1),
			Selector: &metav1.LabelSelector{
				MatchLabels: map[string]string{
					flowLabelKey: flowName,
				},
			},
			Template: corev1.PodTemplateSpec{
				ObjectMeta: metav1.ObjectMeta{
					Labels: map[string]string{
						flowLabelKey: flowName,
					},
				},
				Spec: corev1.PodSpec{
					Containers: []corev1.Container{
						{
							Name:            "runtime",
							Image:           image,
							ImagePullPolicy: imagePullPolicy,
							Args:            []string{runtimeConfigPath},
							Env:             runtimeEnv(),
							VolumeMounts: []corev1.VolumeMount{
								{
									Name:      "flow-config",
									MountPath: runtimeConfigMountDir,
								},
							},
						},
					},
					Volumes: []corev1.Volume{
						{
							Name: "flow-config",
							VolumeSource: corev1.VolumeSource{
								ConfigMap: &corev1.ConfigMapVolumeSource{
									LocalObjectReference: corev1.LocalObjectReference{Name: configMapName},
								},
							},
						},
					},
				},
			},
		},
	}

	deployments := client.AppsV1().Deployments(namespace)
	current, err := deployments.Get(ctx, name, metav1.GetOptions{})
	if err != nil {
		if errors.IsNotFound(err) {
			_, err = deployments.Create(ctx, desired, metav1.CreateOptions{})
			return name, err
		}
		return name, err
	}

	current.Spec = desired.Spec
	current.Labels = desired.Labels
	_, err = deployments.Update(ctx, current, metav1.UpdateOptions{})
	return name, err
}

func applyJob(
	ctx context.Context,
	client kubernetes.Interface,
	namespace string,
	name string,
	configMapName string,
	image string,
	imagePullPolicy corev1.PullPolicy,
) (string, error) {
	desired := &batchv1.Job{
		ObjectMeta: metav1.ObjectMeta{
			Name:      name,
			Namespace: namespace,
			Labels: map[string]string{
				flowLabelKey: name,
			},
		},
		Spec: batchv1.JobSpec{
			Template: corev1.PodTemplateSpec{
				ObjectMeta: metav1.ObjectMeta{
					Labels: map[string]string{
						flowLabelKey: name,
					},
				},
				Spec: corev1.PodSpec{
					RestartPolicy: corev1.RestartPolicyNever,
					Containers: []corev1.Container{
						{
							Name:            "runtime",
							Image:           image,
							ImagePullPolicy: imagePullPolicy,
							Args:            []string{runtimeConfigPath},
							Env:             runtimeEnv(),
							VolumeMounts: []corev1.VolumeMount{
								{
									Name:      "flow-config",
									MountPath: runtimeConfigMountDir,
								},
							},
						},
					},
					Volumes: []corev1.Volume{
						{
							Name: "flow-config",
							VolumeSource: corev1.VolumeSource{
								ConfigMap: &corev1.ConfigMapVolumeSource{
									LocalObjectReference: corev1.LocalObjectReference{Name: configMapName},
								},
							},
						},
					},
				},
			},
		},
	}

	jobs := client.BatchV1().Jobs(namespace)
	_, err := jobs.Get(ctx, name, metav1.GetOptions{})
	if err != nil {
		if errors.IsNotFound(err) {
			_, err = jobs.Create(ctx, desired, metav1.CreateOptions{})
		}
		return name, err
	}

	return name, nil
}

func runtimeEnv() []corev1.EnvVar {
	return []corev1.EnvVar{
		{Name: "FLOWPIPE_METRICS_ENABLED", Value: "true"},
		{Name: "FLOWPIPE_TRACING_ENABLED", Value: "true"},
		{Name: "FLOWPIPE_LOGS_ENABLED", Value: "true"},
		{
			Name: "FLOWPIPE_OTEL_METRICS_ENDPOINT",
			ValueFrom: &corev1.EnvVarSource{
				ConfigMapKeyRef: &corev1.ConfigMapKeySelector{
					LocalObjectReference: corev1.LocalObjectReference{Name: "flow-pipe-observability"},
					Key:                  "metricsEndpoint",
					Optional:             boolPtr(true),
				},
			},
		},
		{
			Name: "FLOWPIPE_OTEL_TRACING_ENDPOINT",
			ValueFrom: &corev1.EnvVarSource{
				ConfigMapKeyRef: &corev1.ConfigMapKeySelector{
					LocalObjectReference: corev1.LocalObjectReference{Name: "flow-pipe-observability"},
					Key:                  "tracesEndpoint",
					Optional:             boolPtr(true),
				},
			},
		},
		{
			Name: "FLOWPIPE_OTEL_LOGS_ENDPOINT",
			ValueFrom: &corev1.EnvVarSource{
				ConfigMapKeyRef: &corev1.ConfigMapKeySelector{
					LocalObjectReference: corev1.LocalObjectReference{Name: "flow-pipe-observability"},
					Key:                  "logsEndpoint",
					Optional:             boolPtr(true),
				},
			},
		},
	}
}

func int32Ptr(v int32) *int32 {
	return &v
}

func boolPtr(v bool) *bool {
	return &v
}
