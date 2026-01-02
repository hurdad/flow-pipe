# flow-pipe Runtime

The **runtime** is the high-performance C++ data plane that executes flow stages. It focuses on predictable throughput and low-latency queueing while remaining stateless and easy to scale.

## Responsibilities
- Load compiled flow graphs and run stages (sources, transforms, sinks).
- Manage bounded in-memory queues between stages.
- Emit metrics and traces for observability without storing user data.

## Layout
- `cmd/` – Runtime entrypoints and launch wrappers.
- `src/` – Core execution engine, schedulers, and stage implementations.
- `include/` – Public headers shared across runtime components.
- `docker/` – Container build context for distributing the runtime image.
- `CMakeLists.txt` – Build definition used by the top-level `make` targets.

## Building
Use the top-level make targets (they run CMake/Ninja under the hood):

```bash
make configure   # one-time CMake configure (creates build directory)
make runtime     # build the runtime binaries
make install     # optional: install runtime artifacts
```

All build artifacts are produced under the top-level `build/` directory.
