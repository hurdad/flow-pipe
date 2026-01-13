# flowctl

`flowctl` is the command-line interface for **flow-pipe**.

It is used to:
- validate flow definitions
- run flows locally during development
- submit flows to the flow-pipe API for execution
- manage flows in the flow-pipe API (get, list, status, update, rollback, delete)
- manage schema registry entries (create, get, list, delete)

The same flow definition is intended to work **locally and remotely** without modification.

---

## Repository layout (CLI)

```
cli/
├── cmd/
│   └── flowctl/
│       ├── main.go
│       ├── root.go
│       ├── run.go
│       ├── get.go
│       ├── list.go
│       ├── status.go
│       ├── submit.go
│       ├── update.go
│       ├── rollback.go
│       ├── stop.go
│       └── validate.go
├── internal/
│   └── loader/           # YAML → FlowSpec loader
├── go.mod
├── go.sum
└── Dockerfile
```

The CLI is a **separate Go module** with its own dependencies. Shared protobuf types and validation helpers are imported from the repository root (`gen/` and `pkg/`).

---

## Building locally

From the **repository root**:

```bash
go build ./cli/cmd/flowctl
```

Or rely on the consolidated build that regenerates protobufs first:

```bash
make cli
```

Or from inside the CLI module:

```bash
cd cli
go build ./cmd/flowctl
```

The binary will be created in the current directory unless `-o` is specified.

---

## Running

```bash
./flowctl --help
```

---

## Docker

A multi-stage Dockerfile is provided to build a minimal image.

### Build the image

From the repository root:

```bash
docker build -f cli/Dockerfile -t flowctl .
```

### Run

```bash
docker run --rm flowctl --help
```

The final image:
- contains only the `flowctl` binary
- runs as a non-root user
- has no shell or package manager (distroless)

---

## Commands

### `validate`

Validate a flow definition without executing it.

```bash
flowctl validate flow.yaml
```

Behavior:
- load and normalize the flow definition
- perform semantic validation
- exit non-zero on failure

This command is designed for **CI usage**.

---

### `run`

Run a flow locally using the flow-pipe runtime.

```bash
flowctl run flow.yaml
```

What happens:
- `flowctl` loads, normalizes, and validates the flow
- the normalized `FlowSpec` is encoded to JSON
- JSON is streamed to a `flow_runtime` executable on your `PATH`

Ensure `flow_runtime` and the stage plugins are installed locally (see `runtime/` and `stages/`).

---

### `submit`

Submit a flow to the flow-pipe API.

```bash
flowctl submit flow.yaml --api localhost:9090
```

Behavior:
- validate the flow locally
- send it to the API over gRPC
- print a success message if the request completes

Flags:
- `--api` – API address (defaults to `localhost:9090`)
- `--timeout` – gRPC request timeout (default 5s)

---

### `get`

Fetch a flow from the API.

```bash
flowctl get flow-name --api localhost:9090
```

Behavior:
- fetch the active flow definition
- print the flow as JSON (proto field names)

---

### `list`

List all flows from the API.

```bash
flowctl list --api localhost:9090
```

Behavior:
- fetch all flows
- print the list as JSON (proto field names)

---

### `status`

Fetch the status of a flow.

```bash
flowctl status flow-name --api localhost:9090
```

Behavior:
- fetch the current flow status
- print the status as JSON (proto field names)

---

### `schema`

Manage schema registry entries via the API.

#### `schema create`

Create a new schema version.

```bash
flowctl schema create registry-id \\
  --format json \\
  --schema-file schema.json \\
  --api localhost:9090
```

Use `--schema-file -` to read from stdin.

#### `schema get`

Fetch a specific schema version.

```bash
flowctl schema get registry-id 1 --api localhost:9090
```

#### `schema list`

List all versions for a schema registry id.

```bash
flowctl schema list registry-id --api localhost:9090
```

#### `schema delete`

Delete all versions for a schema registry id.

```bash
flowctl schema delete registry-id --api localhost:9090
```

---

### `update`

Update an existing flow definition.

```bash
flowctl update flow.yaml --api localhost:9090
```

Behavior:
- validate the flow locally
- update the flow in the API
- print a success message if the request completes

Flags:
- `--api` – API address (defaults to `localhost:9090`)
- `--timeout` – gRPC request timeout (default 5s)
- `--name` – override the flow name (defaults to the spec name)

---

### `rollback`

Rollback a flow to a previous version.

```bash
flowctl rollback flow-name --version 3 --api localhost:9090
```

Behavior:
- request a rollback to the specified version
- print the updated flow as JSON (proto field names)

Flags:
- `--api` – API address (defaults to `localhost:9090`)
- `--timeout` – gRPC request timeout (default 5s)
- `--version` – flow version to roll back to (required)

---

### `stop`

Stop (delete) a flow.

```bash
flowctl stop flow-name --api localhost:9090
```

Behavior:
- delete the flow in the API
- print a success message if the request completes

Flags:
- `--api` – API address (defaults to `localhost:9090`)
- `--timeout` – gRPC request timeout (default 5s)

---

## Design principles

- **Single flow format** (YAML → protobuf)
- **Deterministic normalization**
- **Shared semantics** with API and controller
- **Stateless CLI**
- **Explicit dependencies** (per-module `go.mod`)

---

## Dependency model

The CLI is its own Go module:

```
module github.com/hurdad/flow-pipe/cli
```

It:
- depends on generated protobufs in `gen/`
- depends on shared flow helpers in `pkg/`
- does **not** depend on API or controller internals
- has isolated dependencies via its own `go.mod`

This allows:
- faster builds
- smaller Docker images
- independent evolution of CLI dependencies

---

## Development notes

### Module commands

Run these from inside `cli/`:

```bash
go mod tidy
go mod download
```

Run these from the repo root:

```bash
go build ./cli/cmd/flowctl
```

---

## License

Apache 2.0 (see repository root `LICENSE`).
