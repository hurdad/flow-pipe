# Deployment

This guide covers two common workflows:

- Running a flow locally (single machine, no Kubernetes).
- Deploying the control plane to Kubernetes and submitting flows to it.

## Running locally

Local execution uses the C++ runtime directly and is the fastest way to iterate on
flow definitions and stage plugins.

### 1) Build the runtime + CLI

From the repository root:

```bash
make configure
make runtime
make cli
```

The runtime binary will be under `cmake-build/runtime/flow_runtime`, and `flowctl`
will be built from the CLI module.

### 2) Install or point at stage plugins

The runtime loads plugins from `/opt/flow-pipe/plugins` by default. You can either:

- Install locally (requires write access to `/opt/flow-pipe`):

  ```bash
  sudo make install
  ```

- Or reference absolute plugin paths in your flow YAML via the `plugin` field.

### 3) Run a sample flow

```bash
./flowctl run flows/noop.yaml
```

If the runtime is not on your `PATH`, call it directly instead:

```bash
./cmake-build/runtime/flow_runtime flows/noop.yaml
```

## Running on Kubernetes

Kubernetes deployments use the Go API + controller to manage flows, with runtimes
created as Deployments (streaming) or Jobs (run-to-completion).

### 1) Create a cluster

Use your preferred local cluster. Examples:

```bash
kind create cluster --name flow-pipe
```

### 2) Install the Helm chart

From the repository root:

```bash
helm upgrade --install flow-pipe deploy/helm/flow-pipe
```

This installs etcd, the API, and the controller into the `flow-pipe` namespace
(configurable via `values.yaml`).

### 3) Connect to the API

For local clusters, port-forward the API gRPC service:

```bash
kubectl port-forward svc/flow-pipe-api 9090:9090 -n flow-pipe
```

### 4) Submit a flow

```bash
./flowctl submit flows/noop.yaml --api localhost:9090
```

The controller will create the runtime Deployment/Job and mount the rendered
configuration via ConfigMap.

### Etcd service exposure

When deploying with the Helm chart (`deploy/helm/flow-pipe`), the etcd service type can
be configured through `etcd.service.type`. The default is `ClusterIP`, but it can be
switched to `LoadBalancer` for environments such as k3s with ServiceLB.

## Resource management

Flows declare resource intent:

```yaml
resources:
  cpu:
    cores: 4
  memory:
    mb: 8192
