# ============================================================
# flow-pipe multi-target image
#
# Targets:
#   - runtime: minimal runtime image
#   - base:    runtime image alias for downstream layering
#   - dev:     SDK image for building custom stages
# ============================================================

# ============================================================
# Stage 1: Build + install flow-pipe runtime & stages
# ============================================================
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ARG FLOWPIPE_ENABLE_OTEL=ON

# ------------------------------------------------------------
# Build dependencies
# ------------------------------------------------------------
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    libyaml-cpp-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libgrpc++-dev \
    libcurl4-openssl-dev \
    libspdlog-dev \
    libjemalloc-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# Build workspace
# ------------------------------------------------------------
WORKDIR /build

# Copy source tree
COPY . .

# ------------------------------------------------------------
# Configure
# ------------------------------------------------------------
RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=ON \
      -DFLOWPIPE_ENABLE_OTEL=${FLOWPIPE_ENABLE_OTEL} \
      -DCMAKE_INSTALL_PREFIX=/opt/flow-pipe

# ------------------------------------------------------------
# Build everything (runtime + in-tree stages)
# ------------------------------------------------------------
RUN cmake --build build

# ------------------------------------------------------------
# Run tests
# ------------------------------------------------------------
RUN ctest --test-dir build --output-on-failure

# ------------------------------------------------------------
# Install runtime, shared libs, headers, plugins
# ------------------------------------------------------------
RUN cmake --install build

# ============================================================
# Stage 2: Minimal runtime image
# ============================================================
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# ------------------------------------------------------------
# Runtime-only shared library deps
# ------------------------------------------------------------
RUN apt-get update && apt-get install -y \
    libyaml-cpp0.8 \
    libgrpc++1.51t64 \
    libprotobuf32 \
    libspdlog1.12 \
    libcurl4t64 \
    libjemalloc2 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# ------------------------------------------------------------
# Copy installed runtime from builder
# ------------------------------------------------------------
COPY --from=builder /opt/flow-pipe /opt/flow-pipe

# ------------------------------------------------------------
# Environment
# ------------------------------------------------------------
ENV FLOW_PIPE_ROOT=/opt/flow-pipe
ENV FLOW_PIPE_PLUGIN_PATH=/opt/flow-pipe/plugins
ENV LD_LIBRARY_PATH=/opt/flow-pipe/lib
ENV PATH=/opt/flow-pipe/bin:${PATH}

# ------------------------------------------------------------
# Sanity checks (fail fast)
# ------------------------------------------------------------
RUN test -f /opt/flow-pipe/bin/flow_runtime
RUN test -f /opt/flow-pipe/lib/libflowpipe_runtime.so
RUN ls /opt/flow-pipe/plugins/libstage_*.so

# ------------------------------------------------------------
# Entrypoint
# ------------------------------------------------------------
ENTRYPOINT ["/opt/flow-pipe/bin/flow_runtime"]

# ============================================================
# Stage 3: Base image
# ============================================================
FROM runtime AS base

# ============================================================
# Stage 4: SDK / Dev image
# ============================================================
FROM ubuntu:24.04 AS dev

ENV DEBIAN_FRONTEND=noninteractive

# -----------------------------------------------------
# System dependencies
# -----------------------------------------------------
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    libyaml-cpp-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libgrpc++-dev \
    libgtest-dev \
    libcurl4-openssl-dev \
    libspdlog-dev \
    libjemalloc-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# -----------------------------------------------------
# Workdir
# -----------------------------------------------------
WORKDIR /src

# -----------------------------------------------------
# Copy source (expects submodules already present)
# -----------------------------------------------------
COPY . .

# -----------------------------------------------------
# Install SDK (runtime, libs, headers, plugins)
# -----------------------------------------------------
COPY --from=builder /opt/flow-pipe /opt/flow-pipe

# -----------------------------------------------------
# Sanity checks (fail fast)
# -----------------------------------------------------
# Runtime
RUN test -f /opt/flow-pipe/bin/flow_runtime
RUN test -f /opt/flow-pipe/lib/libflowpipe_runtime.so

# Headers
RUN test -f /opt/flow-pipe/include/flowpipe/stage.h
RUN test -f /opt/flow-pipe/include/flowpipe/plugin.h

# Plugins (examples)
RUN ls /opt/flow-pipe/plugins/libstage_*.so

# -----------------------------------------------------
# Environment (dev + downstream stage builds)
# -----------------------------------------------------
ENV FLOW_PIPE_ROOT=/opt/flow-pipe
ENV CMAKE_PREFIX_PATH=/opt/flow-pipe
ENV LD_LIBRARY_PATH=/opt/flow-pipe/lib
ENV PATH=/opt/flow-pipe/bin:${PATH}
