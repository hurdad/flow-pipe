# flow-pipe Protobufs

This directory contains the protocol buffer definitions for Flow and controller APIs. These files define the schemas shared between the Go control plane and the C++ runtime.

## Structure
- `flowpipe/v1/flow.proto` – FlowSpec definitions, stages, and queue configuration.
- `flowpipe/v1/service.proto` – gRPC service definitions for submitting and inspecting flows.
- `flowpipe/v1/observability.proto` – Telemetry events and metrics emitted by the runtime.

## Code Generation
Use the top-level make targets to generate language bindings:

```bash
make configure   # one-time CMake configure
make proto       # generate Go + C++ bindings
make proto-go    # generate Go bindings in gen/go
make proto-cpp   # generate C++ bindings in cmake-build/proto (used by runtime)
```

Generated artifacts are consumed by `api/`, `controller/`, and the C++ runtime without editing these source `.proto` files directly.
