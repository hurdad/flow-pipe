# flow-pipe API

This directory contains the **flow-pipe control plane API** implemented in Go.

The API is responsible for:
- accepting **FlowSpec** definitions over gRPC / REST
- versioning flows immutably
- storing desired state in **etcd**
- exposing flow status and history

It **does not**:
- schedule workloads
- manage Kubernetes resources
- update flow status (controller-owned)

This mirrors the Kubernetes API / controller separation.

---

## Responsibilities

### The API **owns**
- flow creation
- flow updates (versioned, immutable)
- rollback (pointer update only)
- flow deletion intent
- schema validation
- etcd writes for:
  - `/flows/*/versions/*/spec`
  - `/flows/*/active`
  - `/flows/*/meta`
  - `/schemas/*/versions/*`
  - `/schemas/*/active`

### The API **never**
- writes `/status`
- talks to the runtime
- watches etcd
- parses YAML internally (the CLI handles YAML → protobuf)

---

## Directory Layout

```
api/
├── cmd/
│   └── flow-api/          # Entry point (main.go)
├── internal/
│   ├── config/            # Flag + environment configuration
│   ├── model/             # Flow metadata types
│   ├── server/            # gRPC + HTTP servers
│   ├── service/           # FlowService implementation
│   └── store/             # etcd-backed storage layer
├── go.mod
├── go.sum
└── README.md
```

The API uses generated protobuf types from `gen/` in the repository root.

---

## API Model

### Flow Versioning

- Every update creates a **new immutable version**
- Old versions are never overwritten
- Rollback switches the **active pointer**

```
/flowpipe/flows/<name>/
├── versions/
│   ├── 1/spec
│   ├── 2/spec
│   └── 3/spec
├── active      -> "3"
├── meta
└── status      (controller-owned)
```

### Schema Registry

Queue schemas are stored in etcd so that `QueueSchema.registry_id` and
`QueueSchema.version` can reference an immutable payload. The registry uses
`QueueSchemaFormat` to identify the encoding format and stores the raw schema
payload as bytes (JSON-encoded in etcd for the API implementation).

```
/flowpipe/schemas/<registry_id>/
├── versions/
│   ├── 1
│   ├── 2
│   └── 3
└── active      -> "3"
```

Populate queues with:
- `QueueSchema.format` = one of the `QueueSchemaFormat` enum values (Avro, JSON Schema, Protobuf, etc.)
- `QueueSchema.registry_id` = registry key for the schema
- `QueueSchema.version` = optional version (omit/zero to resolve the active schema)

---

## Transport

### gRPC (authoritative)
- Binary Protobuf
- Used by controllers and internal clients

### REST (via grpc-gateway)
- JSON over HTTP
- Intended for:
  - `flowctl`
  - UI
  - curl-based workflows

Example:
```
POST /v1/flows
```

Body contains **only** `FlowSpec`.

---

## Prerequisites

### Required
- Go ≥ 1.22
- Docker (for etcd)
- etcd ≥ 3.5

### Optional (for local dev)
- `grpcurl`
- `curl`

---

## Running etcd Locally

```bash
docker run -d \
  --name etcd \
  -p 2379:2379 \
  quay.io/coreos/etcd:v3.5.15 \
  etcd \
  --advertise-client-urls http://0.0.0.0:2379 \
  --listen-client-urls http://0.0.0.0:2379
```

---

## Build the API Binary (Local)

From repository root:

```bash
cd api
go build ./cmd/flow-api
```

Or use the top-level helper that ensures protobufs are generated first:

```bash
make api
```

---

## Build Docker Image (Manual)

The API image is built from `api/Dockerfile`.

```bash
docker build -t flow-pipe-api:latest -f api/Dockerfile .
```

---

## Configuration

The API can be configured using **environment variables** or **CLI flags**. Environment variables are recommended for Docker and Kubernetes deployments.

### Environment Variables

| Variable | Description | Default |
|--------|-------------|---------|
| `FLOW_HTTP_ADDR` | HTTP (grpc-gateway) listen address | `:8080` |
| `FLOW_GRPC_ADDR` | gRPC listen address | `:9090` |
| `ETCD_ENDPOINTS` | Comma-separated etcd endpoints | `http://127.0.0.1:2379` |

### CLI Flags

| Flag | Description |
|-----|-------------|
| `-http-addr` | HTTP listen address |
| `-grpc-addr` | gRPC listen address |
| `-etcd` | Comma-separated etcd endpoints |

Environment variables provide the defaults; flags override for local debugging.

---

## Running Locally

With etcd running:

```bash
cd api
FLOW_HTTP_ADDR=:8080 \
FLOW_GRPC_ADDR=:9090 \
ETCD_ENDPOINTS=http://127.0.0.1:2379 \
go run ./cmd/flow-api
```

The process listens on both gRPC and HTTP ports concurrently and shuts down cleanly on SIGINT/SIGTERM.

---

## Running with Docker Compose

Example service definition:

```yaml
flow-api:
  image: flow-pipe-api:latest
  build:
    context: .
    dockerfile: api/Dockerfile
  ports:
    - "8080:8080"
    - "9090:9090"
  depends_on:
    etcd:
      condition: service_healthy
  environment:
    ETCD_ENDPOINTS: "http://etcd:2379"
```

---

## Next Components (Not Here)

This API expects:
- a **controller** watching etcd
- a **runtime** executing flows

Those live outside this directory.

---

## Development Rules (Do Not Break)

- ❌ Never mutate old versions
- ❌ Never write status from API
- ❌ Never accept client-provided version
- ✅ All writes go through etcd transactions
- ✅ Rollback = pointer update only
