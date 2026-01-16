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

Disable observability exporters for the API/controller (and runtime pods launched by the controller) by setting `observability.enabled=false` in Helm values.

## Grafana Dashboard

Import `deploy/grafana/flowpipe-api-dashboard.json` into Grafana to get a starter dashboard for the Flow-Pipe API with metrics, logs, and traces panels. The dashboard expects Prometheus, Loki, and Tempo data sources plus a `service_name` label of `flow-api` (the default `OTEL_SERVICE_NAME`).

Import `deploy/grafana/flowpipe-controller-dashboard.json` into Grafana to get a starter dashboard for the Flow-Pipe controller, including reconcile/queue metrics, logs, and traces. It expects Prometheus, Loki, and Tempo data sources plus a `service_name` label of `flow-controller` (the controller default `OTEL_SERVICE_NAME`).
