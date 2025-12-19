# Flows

This directory contains **flow definitions** for **flow-pipe**.

Flows are written in **YAML** and describe:
- queues
- stages
- wiring between stages

There is **no code** in this directory.

---

## Running a Flow

From the project root:

```bash
flow_runtime flows/noop.yaml
```

Flows assume that the required **stage plugins** are available on the system.

By default, the runtime loads plugins from:

```
/opt/flow-pipe/plugins
```

---

## Plugin Requirements

Each stage in a flow references a **stage type**:

```yaml
type: noop_source
```

At runtime, this maps to a shared library:

```
libstage_noop_source.so
```

The following plugins are expected for the flows in this directory:

| Stage type | Plugin file |
|----------|-------------|
| `noop_source` | `libstage_noop_source.so` |
| `noop_transform` | `libstage_noop_transform.so` |
| `stdout_sink` | `libstage_stdout_sink.so` |

---

## Flow Structure

All flows follow the same structure:

```yaml
queues:
  - name: q1
    capacity: 256

stages:
  - name: source
    type: noop_source
    threads: 1
    output_queue: q1

  - name: sink
    type: stdout_sink
    threads: 1
    input_queue: q1
```

### Queues

Queues define bounded, in-memory channels between stages.

- `name` must be unique
- `capacity` controls backpressure

### Stages

Stages describe how data flows through the pipeline.

Common fields:
- `name` – unique stage identifier
- `type` – stage implementation (plugin)
- `threads` – number of worker threads

Optional fields:
- `input_queue`
- `output_queue`
- `params`

---

## Available Flows

| File | Description |
|----|------------|
| `noop.yaml` | Minimal source → sink pipeline |
| `simple_pipeline.yaml` | Source → transform → sink |
| `fanout.yaml` | Fan-out to multiple transforms and sinks |

---

## Notes

- Stage plugins are loaded **once per stage type**
- Each stage definition creates a new stage instance
- Stages may run with multiple threads
- YAML is converted to an internal protobuf representation at startup

---

## Troubleshooting

### Plugin not found

If you see:

```
dlopen failed: libstage_xxx.so: cannot open shared object file
```

Ensure the plugin exists in:

```
/opt/flow-pipe/plugins
```

and has the correct name.

### Invalid wiring

If a stage has incompatible inputs or outputs, the runtime will fail