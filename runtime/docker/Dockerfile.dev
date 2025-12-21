# =====================================================
# flow-pipe SDK / Dev Image
# =====================================================

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# -----------------------------------------------------
# System dependencies
# -----------------------------------------------------

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    clang \
    git \
    pkg-config \
    ca-certificates \
    libyaml-cpp-dev \
    libprotobuf-dev \
    protobuf-compiler \
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
# Configure + build specific target
# -----------------------------------------------------

RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_INSTALL_PREFIX=/opt/flow-pipe \
 && cmake --build build --target flow_runtime

# -----------------------------------------------------
# Install SDK (headers + libs)
# -----------------------------------------------------

RUN cmake --install build

# -----------------------------------------------------
# Sanity checks (fail fast)
# -----------------------------------------------------

RUN test -f /opt/flow-pipe/include/flowpipe/stage.h
RUN test -f /opt/flow-pipe/bin/flow_runtime

# -----------------------------------------------------
# Environment (nice for dev & downstream plugin builds)
# -----------------------------------------------------

ENV FLOW_PIPE_ROOT=/opt/flow-pipe
ENV CMAKE_PREFIX_PATH=/opt/flow-pipe
ENV LD_LIBRARY_PATH=/opt/flow-pipe/lib:${LD_LIBRARY_PATH}
ENV PATH=/opt/flow-pipe/bin:${PATH}
