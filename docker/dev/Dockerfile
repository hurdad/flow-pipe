# ============================================================
# flow-pipe dev (SDK) image
#
# Purpose:
# - Build flow-pipe stages and other extensions
# - Provide headers, CMake config, and full toolchain
#
# Contains:
# - Everything from flow-pipe-base
# - flow-pipe headers
# - CMake / Ninja / compiler toolchain
# - protobuf / gRPC compilers
#
# Does NOT:
# - Run production workloads directly
#
# CUDA analogy:
#   nvidia/cuda:<ver>-devel
# ============================================================

# ARGs used in FROM must be declared before FROM
ARG FLOW_PIPE_BASE_TAG=main-ubuntu24.04

FROM ghcr.io/hurdad/flow-pipe-base:${FLOW_PIPE_BASE_TAG}

# Install build toolchain + development dependencies
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

# Install flow-pipe SDK (headers + CMake config)
# This directory should be produced by your build/install step:
#   install/opt/flow-pipe/
#
# Expected contents:
#   /opt/flow-pipe/include/flow_pipe/...
#   /opt/flow-pipe/lib/cmake/flow-pipe/flow-pipeConfig.cmake
#FIXMECOPY install/opt/flow-pipe /opt/flow-pipe

# Make flow-pipe discoverable by CMake find_package()
ENV CMAKE_PREFIX_PATH=/opt/flow-pipe

# No ENTRYPOINT override:
# - inherited ENTRYPOINT ["flow_runtime"] from runtime/base
# - acceptable for dev containers
# - can be overridden by `docker run --entrypoint`
