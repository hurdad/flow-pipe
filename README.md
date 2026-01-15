# flow-pipe

**flow-pipe** is a Kubernetes-native pipeline runtime for **streaming** and **run-to-completion** data flows. It combines a high-performance **C++ data plane** with a **Go-based control plane** that deploys and manages flows declaratively on Kubernetes.

flow-pipe is designed to be simple, predictable, and operationally boring.

---

## Why flow-pipe?

Most data platforms end up maintaining:
- long-running ingestion services
- ad-hoc batch jobs for backfills
- custom glue code between queues, files, and sinks

flow-pipe standardizes this pattern around a single abstraction:

> **A pipe that moves data through stages.**

A pipe can:
- run forever (streaming)
- or run to completion (job)
- without changing the pipeline model

---

## Core Concepts

- **Flow** – Declarative pipeline definition
- **Stage** – Unit of work (source, transform, sink, or side-effect)
- **Queue** – Bounded in-memory channel between stages
- **Runtime** – Stateless C++ process that executes a flow
- **Controller** – Kubernetes reconciler that deploys flows
- **Flow API** – Control-plane API for managing flow specs
- **Schema Registry Service** – Versioned store for queue schemas used by flows

---

## Execution Models

flow-pipe supports two execution modes:

| Mode | Kubernetes Resource | Behavior |
|----|----------------------|---------|
| Streaming | Deployment (default) or DaemonSet | Long-running, continuously processing |
| Job | Job | Run once, drain, and exit |

The pipeline definition is identical in both cases; only the lifecycle differs.

---

## Architecture Overview

```
User / CI / Git
      |
      v
+------------------+
|   Flow API       |  (Go)
|  - validate      |
|  - version       |
|  - write spec    |
|   Schema Registry|  (Go)
|  - store schemas |
|  - version       |
+------------------+
          |
          v
        etcd
          |
          v
+------------------+
|  Controller      |  (Go)
|  - reconcile     |
|  - render K8s    |
+------------------+
          |
          v
   Kubernetes
          |
          v
+------------------+
| Runtime          |  (C++)
|  - build DAG     |
|  - spawn threads |
|  - run pipeline  |
+------------------+
```

The runtime is **Kubernetes-agnostic**. Kubernetes is used only for lifecycle and scheduling.

---

## Schema Registry Service

flow-pipe ships with a schema registry service to manage versioned queue schemas.
Queue specs can reference schemas via `QueueSchema.schema_id` and
`QueueSchema.version` (omit/zero to use the active version). The service exposes
REST endpoints such as `POST /v1/schemas`, `GET /v1/schemas/{schema_id}/versions/{version}`,
and `GET /v1/schemas/{schema_id}/versions` for managing schema payloads.

---

## Example Flow

```yaml
name: trade-ingest
execution:
  mode: EXECUTION_MODE_STREAMING

stages:
  - name: sqs_listener
    type: sqs_listener
    threads: 2
    output_queue: objects

  - name: parser
    type: csv_parser
    threads: 4
    input_queue: objects
    output_queue: trades

  - name: writer
    type: db_writer
    threads: 2
    input_queue: trades

queues:
  - name: objects
    capacity: 1024

  - name: trades
    capacity: 65536
```

---

## Design Principles

- Declarative configuration
- Restart-on-change (no hot reload)
- Idempotent reconciliation
- Stateless runtimes
- Clear control plane / data plane separation
- Kubernetes-native lifecycle management

---

## What flow-pipe Is Not

- Not a workflow engine
- Not a general stream processor
- Not a scheduler
- Not an ETL UI

flow-pipe does **not** provide:
- DAG execution semantics
- step retries or branching
- windows or watermarks
- exactly-once guarantees

These exclusions are deliberate.

---

## Repository Layout

```
flow-pipe/
├── api/           # Go control-plane API (gRPC + HTTP)
├── cli/           # flowctl CLI module
├── controller/    # Go Kubernetes controller
├── deploy/        # Helm chart + Argo CD manifests
├── docs/          # Design documentation
├── flows/         # Example flow definitions (YAML)
├── gen/           # Generated protobuf code
├── pkg/           # Shared Go helpers used by CLI/API/controller
├── proto/         # Protobuf schemas
├── runtime/       # C++ data plane and runtime executable
├── stages/        # Built-in example stage plugins
├── third_party/   # External dependencies vendored for C++ builds
├── tools/         # Build helpers and scripts
```

---

## Building

The repository includes a top-level `Makefile` that wires together CMake and Go builds:

```bash
# Generate protobufs and build all components
make all

# Build just the C++ runtime
make runtime

# Build Go components individually
make api
make controller
make cli
```

CMake defaults to installing the runtime under `/opt/flow-pipe` inside the Docker images. Use `make install` (after `make configure`) to install locally with your chosen prefix.

---

## Examples and Assets

- Sample flow definitions live in [`flows/`](flows/README.md).
- Example stage plugins live in [`stages/`](stages/).
- Example custom stage repo: https://github.com/hurdad/flow-pipe-stage-example (walkthrough for building your own stage).
- Docker images for the runtime and SDK are documented in [`runtime/docker/README.md`](runtime/docker/README.md).

---

## Tools and End-to-End Testing

The [`tools/`](tools/README.md) directory contains helper scripts for local development:

- `e2e-k3s.sh` provisions a local k3s-based environment via Docker Compose, builds runtime/controller/API/CLI images, installs the Helm chart with observability disabled by default, and deploys sample flows through the API to validate controller-driven reconciliation.
- `gen_go_protos.sh` regenerates Go protobufs used by the API, controller, and CLI.

See [`tools/README.md`](tools/README.md) for prerequisites, environment variables, and expected outputs when running the end-to-end helper.

These assets are intended as reference implementations for building and running your own pipelines.
