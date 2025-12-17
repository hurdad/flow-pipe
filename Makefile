# ----------------------------
# Configuration
# ----------------------------
BUILD_DIR := build
PROTO_SCRIPT := tools/gen_protos.sh

.PHONY: all clean proto runtime api controller fmt lint

# ----------------------------
# Default target
# ----------------------------
all: proto runtime api controller

# ----------------------------
# Protobuf generation
# ----------------------------
proto:
	@echo "==> Generating protobufs"
	$(PROTO_SCRIPT)

# ----------------------------
# C++ runtime build
# ----------------------------
runtime:
	@echo "==> Building C++ runtime"
	cmake -S . -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

# ----------------------------
# Go API
# ----------------------------
api:
	@echo "==> Building Flow API"
	cd api && go build ./...

# ----------------------------
# Go Controller
# ----------------------------
controller:
	@echo "==> Building Controller"
	cd controller && go build ./...

# ----------------------------
# Formatting (optional)
# ----------------------------
fmt:
	@echo "==> Formatting code"
	cd api && gofmt -w .
	cd controller && gofmt -w .
	find runtime -name '*.cc' -o -name '*.h' | xargs clang-format -i

# ----------------------------
# Lint (optional)
# ----------------------------
lint:
	cd api && go vet ./...
	cd controller && go vet ./...

# ----------------------------
# Clean
# ----------------------------
clean:
	rm -rf $(BUILD_DIR)
