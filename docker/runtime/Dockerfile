# ============================================================
# Stage 1: Build flow_runtime
# ============================================================
FROM ubuntu:24.04 AS builder

# Install build toolchain + build-time dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    libgrpc++-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Build workspace
WORKDIR /build

# Copy the entire repository (runtime + core libs)
COPY . .

# Configure & build only the runtime target
# (adjust path if your CMake layout differs)
RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
  && cmake --build build --target flow_runtime


# ============================================================
# Stage 2: Minimal runtime image
# ============================================================
FROM ubuntu:24.04

# Install runtime-only shared libraries
RUN apt-get update && apt-get install -y \
    libyaml-cpp0.8 \
    libprotobuf32 \
    libgrpc++1.51 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create plugin directory
RUN mkdir -p /opt/flow-pipe/plugins

# Copy ONLY the runtime binary from the builder stage
COPY --from=builder \
    /build/build/runtime/flow_runtime \
    /usr/local/bin/flow_runtime

# Ensure executable
RUN chmod +x /usr/local/bin/flow_runtime

# Default plugin path
ENV FLOW_PIPE_PLUGIN_PATH=/opt/flow-pipe/plugins

# Entrypoint for containers
ENTRYPOINT ["flow_runtime"]
