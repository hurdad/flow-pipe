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
# Copy API module files
# --------------------------------------------------
COPY api/go.mod api/go.sum ./api/

# --------------------------------------------------
# Download deps (replace ../ now works)
# --------------------------------------------------
RUN cd api && go mod download

# --------------------------------------------------
# Copy source
# --------------------------------------------------
COPY gen ./gen
COPY api ./api

# --------------------------------------------------
# Build binary
# --------------------------------------------------
RUN cd api && \
    CGO_ENABLED=0 GOOS=linux GOARCH=arm64 \
    go build -o /flow-api ./cmd/flow-api

# --------------------------------------------------
# Runtime stage
# --------------------------------------------------
FROM gcr.io/distroless/base-debian12

WORKDIR /

COPY --from=build /flow-api /flow-api

EXPOSE 8080 9090

USER nonroot:nonroot

ENTRYPOINT ["/flow-api"]
