# Deployment

## Controller

The controller is deployed as a Kubernetes Deployment with multiple replicas.
Leader election ensures a single active reconciler.

## Runtime

Each flow is deployed as either:
- a Deployment (streaming)
- a Job (run-to-completion)

Configuration is mounted via ConfigMap.

### Etcd service exposure

When deploying with the Helm chart (`deploy/helm/flow-pipe`), the etcd service type can
be configured through `etcd.service.type`. The default is `ClusterIP`, but it can be
switched to `LoadBalancer` for environments such as k3s with ServiceLB.

## Resource Management

Flows declare resource intent:

```yaml
resources:
  cpu:
    cores: 4
  memory:
    mb: 8192
