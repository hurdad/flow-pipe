# Flows

This directory contains **flow definitions** for **flow-pipe**.

Flows are written in **YAML** (or JSON) and describe:
- queues
- stages
- wiring between stages

There is **no code** in this directory.

---

## Running a Flow

### Local binary

From the project root (after running `make configure` and `make runtime`):

```bash
./cmake-build/runtime/flow_runtime flows/noop.yaml
```

The runtime loads stage plugins from `/opt/flow-pipe/plugins` by default. Ensure the built plugins are installed there (for local builds, `make install` places them under the chosen prefix).

### Docker runtime image

Prebuilt runtime images live in [`runtime/docker`](../runtime/docker/README.md) and expose `flow_runtime` as the container entrypoint. To run a flow without installing the binary locally:

```bash
# Pull or build the runtime image first (all images share the same tag)
TAG=main-ubuntu24.04
IMAGE=ghcr.io/hurdad/flow-pipe-runtime:${TAG}

docker run --rm \
  # Mount sample flows
  -v "$(pwd)/flows:/flows" \
  ${IMAGE} /flows/noop.yaml
```

The runtime image bundles the built-in stage plugins under `/opt/flow-pipe/plugins`. To use custom plugins, mount an additional directory and point the runtime at it by rebuilding with a different install prefix or by replacing the mounted `/opt/flow-pipe/plugins` path:

```bash
docker run --rm \
  # Flow definitions
  -v "$(pwd)/flows:/flows" \
  # Custom plugins
  -v "$(pwd)/my-plugins:/opt/flow-pipe/plugins" \
  ${IMAGE} /flows/simple_pipeline.yaml
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
    capacity: 128

stages:
  - name: src
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
- `config`
- `plugin`

---

## Available Flows

| File | Description |
|----|------------|
| `noop.yaml` | Minimal source → sink pipeline |
| `noop.json` | JSON form of the minimal source → sink pipeline |
| `noop.observability.yaml` | No-op pipeline with observability enabled |
| `noop.k3s.yaml` | No-op pipeline tailored for k3s defaults |
| `noop.observability.k3s.yaml` | No-op pipeline for k3s with observability enabled |
| `simple.pipeline.yaml` | Source → transform → sink |
| `simple.pipeline.observability.k3s.yaml` | Job-mode simple pipeline for k3s with observability enabled |
| `fanout.yaml` | Competing-consumer/load-balanced topology (not broadcast fan-out) |
| `schema.registry.yaml` | Source → transform → sink with schema registry references |

---

## Notes

- Stage plugins are loaded **once per stage type**
- Each stage definition creates a new stage instance
- Stages may run with multiple threads
- YAML is converted to an internal protobuf representation at startup
- A single queue with multiple consumers is **load-balanced**: each record is consumed by one downstream consumer, not duplicated to all consumers

---

## Troubleshooting

### Plugin not found

If you see:

```
dlopen failed: libstage_xxx.so: cannot open shared object file
```

Ensure the plugin exists in `/opt/flow-pipe/plugins` (or your custom install prefix) and has the correct name.

### Invalid wiring

If a stage has incompatible inputs or outputs, the runtime will fail to start. Verify queues, inputs, and outputs line up between stages.
