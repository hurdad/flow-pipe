# flow-pipe

**flow-pipe** is a Kubernetes-native pipeline runtime for **streaming** and **run-to-completion** data flows.

It provides a high-performance **C++ data plane** for executing pipelines and a **Go-based control plane** that deploys and manages those pipelines on Kubernetes using declarative configuration.

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

- **Flow** – A declarative pipeline definition
- **Stage** – A unit of work (source, transform, sink, or side-effect)
- **Queue** – A bounded in-memory channel between stages
- **Runtime** – A stateless C++ process that executes a flow
- **Controller** – A Kubernetes reconciler that deploys flows
- **Flow API** – A control-plane API for managing flow specs

---

## Execution Models

flow-pipe supports two execution modes:

| Mode | Kubernetes Resource | Behavior |
|----|----------------------|---------|
| Streaming | Deployment | Long-running, continuously processing |
| Job | Job | Run once, drain, and exit |

The pipeline definition is identical in both cases.  
Only the lifecycle differs.

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

The runtime is **Kubernetes-agnostic**.  
Kubernetes is used only for lifecycle and scheduling.

---

## Example Flow

```yaml
name: trade-ingest
execution:
  mode: STREAMING

stages:
  - name: sqs_listener
    type: sqs_listener
    threads: 2
    output: objects

  - name: parser
    type: csv_parser
    threads: 4
    input: objects
    output: trades

  - name: writer
    type: db_writer
    threads: 2
    input: trades

queues:
  - name: objects
    type: mpsc
    capacity: 1024

  - name: trades
    type: mpmc
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
├── runtime/        # C++ data plane
├── examples/       # Standalone runtime examples and custom stages
├── controller/     # Go Kubernetes controller
├── api/            # Go Flow API
├── proto/          # Protobuf schemas
├── deploy/         # Kubernetes manifests
├── docs/           # Design documentation
```

---

## Examples

The `examples/` directory contains **standalone, buildable C++ programs** that demonstrate how to use and extend the flow-pipe runtime.

Examples:
- Register custom stages
- Build pipelines programmatically
- Demonstrate queue and concurrency semantics
- Run without Kubernetes or the control plane

These examples:
- Link directly against the C++ runtime
- Do not depend on the controller or API
- Reflect how real runtime integrations are expected to work

They are intended as **reference implementations