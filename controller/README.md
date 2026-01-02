# flow-pipe Controller

The **controller** is the Kubernetes reconciler that deploys and manages flow-pipe runtimes. It translates desired FlowSpecs into Kubernetes resources and reports status back to the API.

## Responsibilities
- Watch FlowSpec resources and reconcile desired state to runtime Deployments/Jobs.
- Manage ConfigMaps, Services, and RBAC needed by running flows.
- Surface runtime status and observability data back to the control plane.

## Layout
- `cmd/flow-controller/` – Controller entrypoint and CLI wiring.
- `internal/controller/` – Reconcilers and flow-specific business logic.
- `internal/kube/` – Kubernetes client helpers and informers.
- `internal/observability/` – Metrics/tracing wiring.
- `internal/config/` – Configuration loading and defaults.
- `internal/store/` – State abstractions backed by the Flow API/etcd.

## Building & Testing

Use the top-level Makefile to build the controller and ensure protobufs are up to date:

```bash
make proto-go       # generate Go bindings consumed by the controller
make controller     # build the controller binaries
make -C controller test  # run Go unit tests within this module
```

The controller module follows standard Go module practices and expects `$GOPATH`-independent builds.
