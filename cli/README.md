# flowctl

`flowctl` is the command-line interface for **flow-pipe**.

It is used to:
- validate flow definitions
- run flows locally during development
- submit flows to the flow-pipe API for execution

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
│       ├── submit.go
│       └── validate.go
├── internal/
│   └── ...              # CLI-only helpers
├── go.mod
├── go.sum
└── Dockerfile
```

The CLI is a **separate Go module** with its own dependencies.

Shared protobuf types are imported from the repository root under `gen/`.

---

## Building locally

From the **repository root**:

```bash
go build ./cli/cmd/flowctl
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

Intended behavior:
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

Intended behavior:
- validate the flow
- convert to protobuf
- execute via the local C++ runtime

> Local execution wiring is under active development.

---

### `submit`

Submit a flow to the flow-pipe API.

```bash
flowctl submit flow.yaml --api http://flow-pipe-api:8080
```

Intended behavior:
- validate the flow
- submit it to the API
- the controller schedules execution

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

