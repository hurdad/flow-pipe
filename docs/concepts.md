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
A bounded, in-memory channel between stages.
- MPSC or MPMC
- Enforces backpressure
- Closed automatically when producers exit

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
