# Tools

This directory contains helper tooling for development and CI.

## End-to-end k3s flow validation

Use [`e2e-k3s.sh`](./e2e-k3s.sh) to provision a local k3s-based Kubernetes cluster, build and load Flow Pipe images, install the Helm chart with observability enabled, and exercise flows end-to-end via the API using the `flow-pipe-cli` container image.

### Prerequisites
- Docker (with Buildx) and kubectl installed locally
- Either k3d available on the PATH **or** Docker Compose for the optional compose-based k3s startup
- Helm 3

### Usage
The script accepts a few environment variables to customize the run:

- `IMAGE_NAMESPACE`: Registry/namespace for local images (default: `ghcr.io/${USER:-local}`)
- `IMAGE_TAG`: Tag applied to all locally built images (default: `local-<timestamp>`)
- `CLUSTER_NAME`: Name of the cluster when using k3d (default: `flow-pipe-e2e`)
- `NAMESPACE`: Kubernetes namespace for the release and flows (default: `flow-pipe`)
- `USE_COMPOSE_K3S`: If set, starts k3s via [`k3s-compose.yaml`](./k3s-compose.yaml) instead of k3d
- `KEEP_CLUSTER`: If set, skips cluster teardown for inspection after the run
- `K3S_KUBECONFIG_DIR`: Directory for the compose-based kubeconfig (default: `${REPO_ROOT}/.k3s-kubeconfig`)

Run the end-to-end suite with the defaults (k3d-based k3s):

```bash
./tools/e2e-k3s.sh
```

Run against the compose-backed k3s instead of k3d:

```bash
USE_COMPOSE_K3S=1 ./tools/e2e-k3s.sh
```

### What the script does
1. Provisions a k3s cluster (k3d by default, compose if `USE_COMPOSE_K3S` is set) and waits for nodes to become ready.
2. Builds the layered runtime images (`runtime`, `base`, `dev`) along with controller, API, and `flow-pipe-cli` images, tagging them with `IMAGE_NAMESPACE/flow-pipe-<component>:IMAGE_TAG` and importing them into the cluster runtime.
3. Installs the Helm chart from `deploy/helm/flow-pipe` with observability components (Prometheus, Loki, Tempo, Grafana, Alloy) enabled and waits for the control plane pods.
4. Generates sample streaming and job flow specs, port-forwards the API service, and submits the flows via the `flow-pipe-cli` container so the controller creates the runtime deployment/job.
5. Waits for the controller-created runtimes to become ready/complete, asserts the runtimes emit expected payloads, runs observability queries (Prometheus, Loki, Tempo, Grafana) that fail if metrics/logs are missing, and appends the results plus Helm status/resources to the GitHub Actions step summary when available.

### Outputs and cleanup
- Flow runtime logs are written to `stream.log` and `job.log` in the repository root.
- Observability query output is written to `observability.log` in the repository root.
- If `KEEP_CLUSTER` is **not** set, the script tears down the k3d cluster or compose stack on exit.
