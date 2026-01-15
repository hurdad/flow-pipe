package controller

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"

	flowpipev1 "github.com/hurdad/flow-pipe/gen/go/flowpipe/v1"
	"google.golang.org/protobuf/encoding/protojson"
	appsv1 "k8s.io/api/apps/v1"
	batchv1 "k8s.io/api/batch/v1"
	corev1 "k8s.io/api/core/v1"
	apierrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"sigs.k8s.io/yaml"
)

const (
	flowLabelKey          = "flowpipe.io/flow-name"
	runtimeConfigKey      = "flow.yaml"
	runtimeConfigMountDir = "/config"
	runtimeConfigPath     = "/config/flow.yaml"
	runtimeConfigHashKey  = "flowpipe.io/flow-config-checksum"
)

func ensureRuntime(
	ctx context.Context,
	client kubernetes.Interface,
	namespace string,
	spec *flowpipev1.FlowSpec,
	imagePullPolicy corev1.PullPolicy,
	observabilityEnabled bool,
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
	if spec.Kubernetes != nil && spec.Kubernetes.Image != nil {
		image = *spec.Kubernetes.Image
	}
	if image == "" {
		return "", fmt.Errorf("runtime image is required for flow %q", spec.Name)
	}

	configMapName := fmt.Sprintf("%s-config", spec.Name)
	configChecksum, err := applyConfigMap(ctx, client, namespace, configMapName, spec)
	if err != nil {
		return "", err
	}

	mode := flowpipev1.ExecutionMode_EXECUTION_MODE_STREAMING
	if spec.Execution != nil {
		mode = spec.Execution.Mode
	}

	switch mode {
	case flowpipev1.ExecutionMode_EXECUTION_MODE_JOB:
		return applyJob(ctx, client, namespace, spec, configMapName, image, imagePullPolicy, configChecksum, observabilityEnabled)
	case flowpipev1.ExecutionMode_EXECUTION_MODE_STREAMING:
		fallthrough
	default:
		deploymentName := fmt.Sprintf("%s-runtime", spec.Name)
		return applyDeployment(ctx, client, namespace, spec.Name, deploymentName, configMapName, image, imagePullPolicy, configChecksum, observabilityEnabled)
	}
}

func applyConfigMap(
	ctx context.Context,
	client kubernetes.Interface,
	namespace string,
	name string,
	spec *flowpipev1.FlowSpec,
) (string, error) {
	payload, err := protojson.MarshalOptions{UseProtoNames: true}.Marshal(spec)
	if err != nil {
		return "", fmt.Errorf("marshal flow spec: %w", err)
	}
	yamlPayload, err := yaml.JSONToYAML(payload)
	if err != nil {
		return "", fmt.Errorf("marshal flow spec as yaml: %w", err)
	}
	checksum := sha256.Sum256(yamlPayload)
	checksumValue := hex.EncodeToString(checksum[:])

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
		if apierrors.IsNotFound(err) {
			_, err = cmClient.Create(ctx, desired, metav1.CreateOptions{})
			return checksumValue, err
		}
		return checksumValue, err
	}

	current.Data = desired.Data
	current.Labels = desired.Labels
	_, err = cmClient.Update(ctx, current, metav1.UpdateOptions{})
	return checksumValue, err
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
	configChecksum string,
	observabilityEnabled bool,
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
					Annotations: map[string]string{
						runtimeConfigHashKey: configChecksum,
					},
				},
				Spec: corev1.PodSpec{
					Containers: []corev1.Container{
						{
							Name:            "runtime",
							Image:           image,
							ImagePullPolicy: imagePullPolicy,
							Args:            []string{runtimeConfigPath},
							Env:             runtimeEnv(observabilityEnabled),
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
	applyKubernetesOptions(&desired.Spec.Template, spec.GetKubernetesOptions())

	deployments := client.AppsV1().Deployments(namespace)
	current, err := deployments.Get(ctx, name, metav1.GetOptions{})
	if err != nil {
		if apierrors.IsNotFound(err) {
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
	spec *flowpipev1.FlowSpec,
	configMapName string,
	image string,
	imagePullPolicy corev1.PullPolicy,
	configChecksum string,
	observabilityEnabled bool,
) (string, error) {
	name := spec.Name
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
					Annotations: map[string]string{
						runtimeConfigHashKey: configChecksum,
					},
				},
				Spec: corev1.PodSpec{
					RestartPolicy: restartPolicyFromSpec(spec),
					Containers: []corev1.Container{
						{
							Name:            "runtime",
							Image:           image,
							ImagePullPolicy: imagePullPolicy,
							Args:            []string{runtimeConfigPath},
							Env:             runtimeEnv(observabilityEnabled),
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
	applyKubernetesOptions(&desired.Spec.Template, spec.GetKubernetesOptions())

	jobs := client.BatchV1().Jobs(namespace)
	_, err := jobs.Get(ctx, name, metav1.GetOptions{})
	if err != nil {
		if apierrors.IsNotFound(err) {
			_, err = jobs.Create(ctx, desired, metav1.CreateOptions{})
		}
		return name, err
	}

	return name, nil
}

func deleteRuntimeResources(
	ctx context.Context,
	client kubernetes.Interface,
	namespace string,
	flowName string,
) error {
	if client == nil {
		return fmt.Errorf("kubernetes client is required for runtime deletion")
	}
	if flowName == "" {
		return fmt.Errorf("flow name is required for runtime deletion")
	}

	var errs []error
	deploymentName := fmt.Sprintf("%s-runtime", flowName)
	if err := client.AppsV1().Deployments(namespace).Delete(ctx, deploymentName, metav1.DeleteOptions{}); err != nil && !apierrors.IsNotFound(err) {
		errs = append(errs, fmt.Errorf("delete deployment %q: %w", deploymentName, err))
	}

	if err := client.BatchV1().Jobs(namespace).Delete(ctx, flowName, metav1.DeleteOptions{}); err != nil && !apierrors.IsNotFound(err) {
		errs = append(errs, fmt.Errorf("delete job %q: %w", flowName, err))
	}

	configMapName := fmt.Sprintf("%s-config", flowName)
	if err := client.CoreV1().ConfigMaps(namespace).Delete(ctx, configMapName, metav1.DeleteOptions{}); err != nil && !apierrors.IsNotFound(err) {
		errs = append(errs, fmt.Errorf("delete configmap %q: %w", configMapName, err))
	}

	if len(errs) > 0 {
		return errors.Join(errs...)
	}
	return nil
}

func pullPolicyFromSpec(spec *flowpipev1.FlowSpec) corev1.PullPolicy {
	if spec == nil {
		return corev1.PullIfNotPresent
	}

	if spec.Kubernetes == nil {
		return corev1.PullIfNotPresent
	}

	switch spec.Kubernetes.ImagePullPolicy {
	case flowpipev1.ImagePullPolicy_IMAGE_PULL_POLICY_ALWAYS:
		return corev1.PullAlways
	case flowpipev1.ImagePullPolicy_IMAGE_PULL_POLICY_NEVER:
		return corev1.PullNever
	case flowpipev1.ImagePullPolicy_IMAGE_PULL_POLICY_IF_NOT_PRESENT:
		fallthrough
	default:
		return corev1.PullIfNotPresent
	}
}

func restartPolicyFromSpec(spec *flowpipev1.FlowSpec) corev1.RestartPolicy {
	if spec == nil {
		return corev1.RestartPolicyNever
	}

	if spec.Kubernetes == nil {
		return corev1.RestartPolicyNever
	}

	switch spec.Kubernetes.RestartPolicy {
	case flowpipev1.RestartPolicy_RESTART_POLICY_ALWAYS:
		return corev1.RestartPolicyAlways
	case flowpipev1.RestartPolicy_RESTART_POLICY_ON_FAILURE:
		return corev1.RestartPolicyOnFailure
	case flowpipev1.RestartPolicy_RESTART_POLICY_NEVER:
		fallthrough
	default:
		return corev1.RestartPolicyNever
	}
}

func runtimeEnv(observabilityEnabled bool) []corev1.EnvVar {
	env := []corev1.EnvVar{
		{Name: "FLOWPIPE_OBSERVABILITY_ENABLED", Value: fmt.Sprintf("%t", observabilityEnabled)},
	}

	if !observabilityEnabled {
		return env
	}

	env = append(env,
		corev1.EnvVar{Name: "FLOWPIPE_METRICS_ENABLED", Value: "true"},
		corev1.EnvVar{Name: "FLOWPIPE_TRACING_ENABLED", Value: "true"},
		corev1.EnvVar{Name: "FLOWPIPE_LOGS_ENABLED", Value: "true"},
		corev1.EnvVar{
			Name: "OTEL_EXPORTER_OTLP_ENDPOINT",
			ValueFrom: &corev1.EnvVarSource{
				ConfigMapKeyRef: &corev1.ConfigMapKeySelector{
					LocalObjectReference: corev1.LocalObjectReference{Name: "flow-pipe-observability"},
					Key:                  "alloyEndpoint",
					Optional:             boolPtr(true),
				},
			},
		},
	)

	return env
}

func applyKubernetesOptions(template *corev1.PodTemplateSpec, options *flowpipev1.KubernetesOptions) {
	if template == nil || options == nil {
		return
	}

	if len(options.PodLabels) > 0 {
		if template.Labels == nil {
			template.Labels = map[string]string{}
		}
		for key, value := range options.PodLabels {
			if key == flowLabelKey {
				continue
			}
			template.Labels[key] = value
		}
	}

	if len(options.PodAnnotations) > 0 {
		if template.Annotations == nil {
			template.Annotations = map[string]string{}
		}
		for key, value := range options.PodAnnotations {
			if key == runtimeConfigHashKey {
				continue
			}
			template.Annotations[key] = value
		}
	}

	if options.ServiceAccountName != nil {
		template.Spec.ServiceAccountName = *options.ServiceAccountName
	}

	if len(options.ImagePullSecrets) > 0 {
		secrets := make([]corev1.LocalObjectReference, 0, len(options.ImagePullSecrets))
		for _, name := range options.ImagePullSecrets {
			if name == "" {
				continue
			}
			secrets = append(secrets, corev1.LocalObjectReference{Name: name})
		}
		template.Spec.ImagePullSecrets = secrets
	}

	if options.RuntimeClassName != nil {
		template.Spec.RuntimeClassName = options.RuntimeClassName
	}
}

func int32Ptr(v int32) *int32 {
	return &v
}

func boolPtr(v bool) *bool {
	return &v
}
