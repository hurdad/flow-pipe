# Flow-Pipe Metrics

Flow-Pipe exposes runtime metrics via **OpenTelemetry Metrics**.
These metrics provide visibility into **queues**, **stages**, and **runtime performance**.

Metrics are emitted only when enabled in the FlowSpec observability configuration and are further gated by runtime flags (for example, latency histograms on/off).

---

## Metric Naming Conventions

All metrics are prefixed with:

```
flowpipe.*
```

Labels are intentionally low-cardinality to ensure safe operation at scale.

---

## Queue Metrics

### `flowpipe.queue.enqueue.count`

- **Type:** Counter (`UInt64Counter`)
- **Description:**  
  Number of payloads successfully enqueued into a queue.
- **Labels:**
    - `queue` – logical queue name
- **Emitted when:**  
  A payload is pushed into a queue by the runtime.

---

### `flowpipe.queue.dequeue.count`

- **Type:** Counter (`UInt64Counter`)
- **Description:**  
  Number of payloads dequeued from a queue.
- **Labels:**
    - `queue` – logical queue name
- **Emitted when:**  
  A payload is popped from a queue.

---

### `flowpipe.queue.dwell_ns`

- **Type:** Histogram (`UInt64Histogram`)
- **Description:**  
  Time (in nanoseconds) a payload spent waiting in a queue.
- **Labels:**
    - `queue` – logical queue name
- **Value:**
  ```
  dequeue_time_ns - payload.meta.enqueue_ts_ns
  ```
- **Emitted when:**  
  A payload is dequeued and has a valid enqueue timestamp.
- **Notes:**
    - Emitted only when latency histograms are enabled.
    - Useful for detecting backpressure and queue saturation.

---

## Stage Metrics

### `flowpipe.stage.process.count`

- **Type:** Counter (`UInt64Counter`)
- **Description:**  
  Number of payloads processed by a stage.
- **Labels:**
    - `stage` – stage name
- **Emitted when:**  
  A stage processes a payload (one invocation).

---

### `flowpipe.stage.latency_ns`

- **Type:** Histogram (`UInt64Histogram`)
- **Description:**  
  Execution latency of a stage per payload.
- **Labels:**
    - `stage` – stage name
- **Value:**
  ```
  end_time_ns - start_time_ns
  ```
- **Emitted when:**  
  A stage completes processing a payload.
- **Notes:**
    - Emitted only when latency histograms are enabled.
    - Correlates directly with tracing spans when tracing is enabled.

---

### `flowpipe.stage.errors`

- **Type:** Counter (`UInt64Counter`)
- **Description:**  
  Number of errors recorded by a stage.
- **Labels:**
    - `stage` – stage name
- **Emitted when:**  
  The runtime records a stage error.

---

## Summary Table

| Metric Name | Type | Labels | Purpose |
|------------|------|--------|--------|
| `flowpipe.queue.enqueue.count` | Counter | `queue` | Queue ingress rate |
| `flowpipe.queue.dequeue.count` | Counter | `queue` | Queue egress rate |
| `flowpipe.queue.dwell_ns` | Histogram | `queue` | Time-in-queue / backpressure |
| `flowpipe.stage.process.count` | Counter | `stage` | Stage throughput |
| `flowpipe.stage.latency_ns` | Histogram | `stage` | Stage execution latency |
| `flowpipe.stage.errors` | Counter | `stage` | Stage error rate |

---

## Trace ↔ Metric Correlation

When tracing is enabled:

- Metrics are recorded with the **active OpenTelemetry context**
- Stage latency metrics align with **stage execution spans**
- Queue dwell metrics correlate with upstream and downstream spans

This allows direct correlation in observability backends such as **Grafana (Tempo + Prometheus)**.

---

## Metrics Not Yet Implemented (Planned)

The following metrics are intentionally deferred but fit naturally into the current design:

- Queue depth (observable gauge)
- Queue capacity (gauge)
- In-flight records per stage
- Stage concurrency
- Backpressure / enqueue blocking time
- Dropped record counters
- End-to-end flow latency

---

## Cardinality & Performance Notes

- All labels are **bounded** (`queue`, `stage`)
- No per-record identifiers are used as labels
- Metrics recording is safe on hot paths
- Histograms can be disabled independently for low-overhead operation

