# Core Concepts

## Flow
A declarative specification describing:
- stages
- queues
- execution mode
- resource intent

Flows are immutable and versioned.

---

## Stage
A unit of execution. A stage may have:
- no input (source)
- no output (sink)
- both (transform)
- neither (side-effect)

Stages implement business logic only. Threading and lifecycle are owned by the runtime.

---

## Queue
A queue is the channel between stages. Flows use the bounded in-memory queue
implementation via `QueueSpec.type`:

- **Bounded in-memory queue (default)**: A capacity-limited, in-memory channel that
  enforces backpressure. This is the default when `QueueSpec.type` is omitted or set
  to `QUEUE_TYPE_IN_MEMORY`.

Queue semantics:
- MPSC or MPMC
- Multiple consumers on one queue are **competing consumers** (load-balanced delivery)
- A single queue does **not** broadcast/duplicate each record to every downstream consumer
- Enforces backpressure
- Closed automatically when producers exit

---

## Schema Registry
Queue schemas are stored in the schema registry service so flows can reference
versioned, immutable payloads. Each schema is identified by a `schema_id`,
with versions managed by the service. Queue specs reference schemas via:

- `QueueSchema.format` for the encoding (Avro, JSON Schema, Protobuf, etc.).
- `QueueSchema.schema_id` to locate the schema definition.
- `QueueSchema.version` to pin a specific version (omit/zero to use the active version).
- `QueueSchema.registry_url` to override the registry base URL when needed.

---

## Execution Modes

### Streaming
- Infinite sources allowed
- Runs until deleted
- Implemented using Kubernetes Deployment

### Job
- Finite sources only
- Pipeline drains and exits
- Implemented using Kubernetes Job
