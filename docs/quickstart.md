# Quick Start

This quick start walks through building flow-pipe, running a local flow, and
(optionally) deploying the control plane on Kubernetes.

## Prerequisites

- Go and a C++ build toolchain (CMake + a C++17 compiler).
- Docker (for container-based builds or local clusters).
- `kubectl` and Helm if you plan to deploy to Kubernetes.

## 1) Build the runtime + CLI

From the repository root:

```bash
make configure
make runtime
make cli
```

The runtime binary will be under `cmake-build/runtime/flow_runtime`, and the
`flowctl` CLI will be built from the `cli` module.

## 2) Run a local flow

Use the built CLI to run the sample noop flow:

```bash
./flowctl run flows/noop.yaml
```

If the runtime is not on your `PATH`, call it directly:

```bash
./cmake-build/runtime/flow_runtime flows/noop.yaml
```

## 3) (Optional) Deploy to Kubernetes

Create a local cluster (example uses kind):

```bash
kind create cluster --name flow-pipe
```

Install the Helm chart:

```bash
helm upgrade --install flow-pipe deploy/helm/flow-pipe
```

Port-forward the API to submit flows:

```bash
kubectl port-forward svc/flow-pipe-api 9090:9090 -n flow-pipe
```

Submit a flow through the control plane:

```bash
./flowctl submit flows/noop.yaml --api localhost:9090
```

## Next steps

- Read about the pipeline model in [Core Concepts](concepts.md).
- Explore deployment details in [Deployment](deployment.md).
- Browse example flow specs under [`flows/`](../flows/README.md).
