# Build stage
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    protobuf-compiler \
    libgrpc++-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j$(nproc)

# Runtime stage
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libyaml-cpp0.8 \
    libprotobuf32 \
    libgrpc++1.51 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/build/runtime/flow_runtime /usr/local/bin/flow_runtime
#COPY --from=builder /build/build/stages /opt/flow-pipe/plugins

RUN mkdir -p /opt/flow-pipe/plugins

COPY --from=builder /build/build/stages/noop_source/libstage_noop_source.so \
    /opt/flow-pipe/plugins/

COPY --from=builder /build/build/stages/noop_transform/libstage_noop_transform.so \
    /opt/flow-pipe/plugins/

COPY --from=builder /build/build/stages/stdout_sink/libstage_stdout_sink.so \
    /opt/flow-pipe/plugins/


ENTRYPOINT ["flow_runtime"]
