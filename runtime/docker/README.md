# flow-pipe Docker Images

This directory contains the Dockerfiles used to build the official
**flow-pipe container images**.

The images are intentionally split into layers, following the same
model used by NVIDIA CUDA images (`runtime`, `base`, `devel`).

This design enforces ABI compatibility, keeps runtime images small,
and provides a clean SDK for building custom stages.

---

## Image Hierarchy

```
flow-pipe-runtime  →  flow-pipe-base  →  flow-pipe-dev
```

Each image builds on the one before it and **all images must share
the same tag**.

| Image | Purpose |
|------|--------|
| `flow-pipe-runtime` | Minimal image to *run* flows |
| `flow-pipe-base` | Runtime + shared library dependencies |
| `flow-pipe-dev` | SDK image for building custom stages |

---

## `Dockerfile.runtime`

**Lowest-level image**.

### Contains
- `flow_runtime` binary
- Plugin loader
- Runtime shared libraries only

### Does NOT contain
- Headers
- Compilers
- Build tools

This is the image used in production Kubernetes workloads.

---

## `Dockerfile.base`

Extends the runtime image.

### Adds
- gRPC runtime libraries
- Protobuf runtime libraries
- YAML runtime libraries
- CA certificates

### Still does NOT contain
- Headers
- Compilers
- Build tools

This image exists to separate runtime dependencies from the core binary
and is the parent of the SDK image.

---

## `Dockerfile.dev` (SDK)

The development image used to build custom flow-pipe stages.

### Adds
- `/opt/flow-pipe/include` (flow-pipe headers)
- CMake configuration (`flow-pipeConfig.cmake`)
- Compiler toolchain
- CMake / Ninja
- Protobuf and gRPC compilers

This image should be used by **external stage repositories**.

---

## Build Order (Local)

Images must be built **in order**.

```bash
# 1. runtime
docker build -f runtime/docker/Dockerfile.runtime \
  -t ghcr.io/hurdad/flow-pipe-runtime:<tag> .

# 2. base
docker build -f runtime/docker/Dockerfile.base \
  --build-arg FLOW_PIPE_RUNTIME_TAG=<tag> \
  -t ghcr.io/hurdad/flow-pipe-base:<tag> .

# 3. dev
docker build -f runtime/docker/Dockerfile.dev \
  --build-arg FLOW_PIPE_BASE_TAG=<tag> \
  -t ghcr.io/hurdad/flow-pipe-dev:<tag> .
```

Example tag:

```
main-ubuntu24.04
0.1.0-ubuntu24.04
```

---

## Tagging Rules (Important)

All images **must share the same tag**.

Correct:
```
flow-pipe-runtime:0.1.0-ubuntu24.04
flow-pipe-base:0.1.0-ubuntu24.04
flow-pipe-dev:0.1.0-ubuntu24.04
```

Incorrect:
```
runtime:0.1.0
base:latest
dev:main
```

Mixing tags breaks ABI compatibility.

---

## External Stage Repositories

Custom stages should **never** be built against the runtime image.

Instead:

```dockerfile
FROM ghcr.io/hurdad/flow-pipe-dev:<tag> AS build
```

Then copy the resulting `.so` into a runtime image.

This guarantees:
- ABI compatibility
- Stable plugin loading
- Predictable deployments

---

## CI / GitHub Actions

The GitHub Actions workflow builds images in the correct order:

```
runtime → base → dev
```

The tag is computed once and reused across all images to ensure
compatibility.

See:
```
.github/workflows/runtime-images.yml
```

---

## Design Philosophy

- Minimal runtime images
- Strict layering
- Explicit ABI boundaries
- No compile-in-production
- Long-term compatibility

This structure is intentionally boring — and that’s a good thing.
