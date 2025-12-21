# --------------------------------------------------
# Build stage
# --------------------------------------------------
FROM golang:1.22-alpine AS build

WORKDIR /src

RUN apk add --no-cache git

# --------------------------------------------------
# Copy ROOT go.mod (required for replace ../)
# --------------------------------------------------
COPY go.mod ./go.mod

# --------------------------------------------------
# Copy Controller module files
# --------------------------------------------------
COPY cli/go.mod cli/go.sum ./cli/

# --------------------------------------------------
# Download deps (replace ../ now works)
# --------------------------------------------------
RUN cd cli && go mod download

# --------------------------------------------------
# Copy source
# --------------------------------------------------
COPY gen ./gen
COPY cli ./cli

# --------------------------------------------------
# Build binary
# --------------------------------------------------
RUN cd cli && \
    CGO_ENABLED=0 \
    go build -o /flowctl ./cmd/flowctl

# --------------------------------------------------
# Runtime stage
# --------------------------------------------------
FROM gcr.io/distroless/base-debian13

WORKDIR /

COPY --from=build /flowctl /flowctl

USER nonroot:nonroot

ENTRYPOINT ["/flowctl"]
