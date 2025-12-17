#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROTO_DIR="${ROOT_DIR}/proto"
GOOGLEAPIS_DIR="${ROOT_DIR}/third_party/googleapis"
OUT_GO="${ROOT_DIR}/gen/go"
OUT_OPENAPI="${ROOT_DIR}/gen/openapi"

# --------------------------------------------------
# Sanity checks
# --------------------------------------------------

if ! command -v protoc >/dev/null 2>&1; then
  echo "protoc not found"
  exit 1
fi

if [ ! -d "${GOOGLEAPIS_DIR}/google/api" ]; then
  echo "googleapis submodule not found."
  echo "Run: git submodule update --init --recursive"
  exit 1
fi

# --------------------------------------------------
# Output dirs
# --------------------------------------------------

mkdir -p "${OUT_GO}"
mkdir -p "${OUT_OPENAPI}"

# --------------------------------------------------
# Generate Go protobuf + gRPC
# --------------------------------------------------

protoc \
  -I "${PROTO_DIR}" \
  -I "${GOOGLEAPIS_DIR}" \
  --go_out="${OUT_GO}" \
  --go_opt=paths=source_relative \
  --go-grpc_out="${OUT_GO}" \
  --go-grpc_opt=paths=source_relative \
  "${PROTO_DIR}/flow.proto" \
  "${PROTO_DIR}/service.proto"

# --------------------------------------------------
# Generate REST gateway + OpenAPI
# --------------------------------------------------

protoc \
  -I "${PROTO_DIR}" \
  -I "${GOOGLEAPIS_DIR}" \
  --grpc-gateway_out="${OUT_GO}" \
  --grpc-gateway_opt=paths=source_relative \
  --openapiv2_out="${OUT_OPENAPI}" \
  "${PROTO_DIR}/service.proto"

echo "Go protobufs generated:"
echo "  - ${OUT_GO}"
echo "OpenAPI spec generated:"
echo "  - ${OUT_OPENAPI}"
