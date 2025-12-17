# -------------------------------------------------
# Build stage
# -------------------------------------------------
FROM ubuntu:24.04 AS build

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    ca-certificates \
    curl \
    pkg-config \
    libyaml-cpp-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy into workir
COPY runtime/ ./

# Configure + build + install
RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/opt/flow-pipe \
 && cmake --build build \
 && cmake --install build

# -------------------------------------------------
# Runtime base stage
# -------------------------------------------------
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    ca-certificates \
    libyaml-cpp0.8 \
    && rm -rf /var/lib/apt/lists/*

# Install runtime artifacts
COPY --from=build /opt/flow-pipe /opt/flow-pipe

# Runtime environment
ENV FLOW_PIPE_HOME=/opt/flow-pipe
ENV PATH="${FLOW_PIPE_HOME}/bin:${PATH}"
ENV LD_LIBRARY_PATH="${FLOW_PIPE_HOME}/lib:${LD_LIBRARY_PATH}"
ENV CMAKE_PREFIX_PATH="${FLOW_PIPE_HOME}:${CMAKE_PREFIX_PATH}"

WORKDIR /app

ENTRYPOINT ["/opt/flow-pipe/bin/flow-runtime"]
