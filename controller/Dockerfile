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
COPY controller/go.mod controller/go.sum ./controller/

# --------------------------------------------------
# Download deps (replace ../ now works)
# --------------------------------------------------
RUN cd controller && go mod download

# --------------------------------------------------
# Copy source
# --------------------------------------------------
COPY gen ./gen
COPY controller ./controller

# --------------------------------------------------
# Build binary
# --------------------------------------------------
RUN cd controller && \
    CGO_ENABLED=0 \
    go build -o /flow-controller ./cmd/flow-controller

# --------------------------------------------------
# Runtime stage
# --------------------------------------------------
FROM gcr.io/distroless/base-debian13

WORKDIR /

COPY --from=build /flow-controller /flow-controller

USER nonroot:nonroot

ENTRYPOINT ["/flow-controller"]
