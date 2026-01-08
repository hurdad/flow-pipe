# Deployment

flow-pipe is deployed via Helm. The manifests in this directory package the control plane (API + controller) and runtime dependencies.

Argo CD is the recommended installation mechanism, but Helm can also be used directly.

- `helm/flow-pipe` contains the platform chart (API, controller, and supporting config)
- `argocd/` contains Argo CD `Application` definitions wiring the chart into a GitOps workflow

To install with plain Helm from the repository root:

```bash
helm upgrade --install flow-pipe deploy/helm/flow-pipe
```

Override values as needed for image tags, etcd endpoints, and Kubernetes namespaces.

The Helm chart only bundles Grafana Alloy for observability. Configure the Alloy OTLP export endpoints for metrics, traces, and logs under `observability.alloy.exporters` to forward telemetry to your backend.
