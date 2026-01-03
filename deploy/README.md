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

When enabling the bundled Grafana chart via `observability.grafana.enabled`, datasources for Prometheus, Loki, and Tempo are automatically provisioned using the configured endpoints for those services. A Flow Pipe runtime dashboard is also shipped and loaded via the Grafana sidecar, surfacing queue throughput, dwell latency percentiles, stage throughput, stage latency percentiles, and stage error rates using the metrics documented in `runtime/METRICS.md`.
