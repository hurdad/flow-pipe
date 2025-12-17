# Deployment

## Controller

The controller is deployed as a Kubernetes Deployment with multiple replicas.
Leader election ensures a single active reconciler.

## Runtime

Each flow is deployed as either:
- a Deployment (streaming)
- a Job (run-to-completion)

Configuration is mounted via ConfigMap.

## Resource Management

Flows declare resource intent:

```yaml
resources:
  cpu:
    cores: 4
  memory:
    mb: 8192
