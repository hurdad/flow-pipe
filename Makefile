# =====================================================
# flow-pipe Makefile
# =====================================================

# Directories
BUILD_DIR := cmake-build
PROTO_GO_SCRIPT := tools/gen_go_protos.sh

# Go binaries
API_DIR := api
CONTROLLER_DIR := controller

# -----------------------------------------------------
# Phony targets
# -----------------------------------------------------

.PHONY: all proto proto-go proto-cpp runtime api controller clean fmt lint submodules

# -----------------------------------------------------
# Default target
# -----------------------------------------------------

all: submodules proto runtime api controller

# -----------------------------------------------------
# Submodules
# -----------------------------------------------------

submodules:
	@git submodule update --init --recursive

# -----------------------------------------------------
# Protobuf
# -----------------------------------------------------

proto: proto-go proto-cpp

proto-go:
	@echo "==> Generating Go protobufs"
	@$(PROTO_GO_SCRIPT)

proto-cpp:
	@echo "==> Generating C++ protobufs and building runtime deps"
	@cmake -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR) --target flowpipe_proto

# -----------------------------------------------------
# Runtime (C++)
# -----------------------------------------------------

runtime:
	@echo "==> Building C++ runtime"
	@cmake -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR)

# -----------------------------------------------------
# Go API
# -----------------------------------------------------

api: proto
	@echo "==> Building Flow API"
	@cd $(API_DIR) && go build ./...

# -----------------------------------------------------
# Go Controller
# -----------------------------------------------------

controller: proto
	@echo "==> Building Flow Controller"
	@cd $(CONTROLLER_DIR) && go build ./...

# -----------------------------------------------------
# Formatting
# -----------------------------------------------------

fmt:
	@echo "==> Formatting code"
	@cd $(API_DIR) && gofmt -w .
	@cd $(CONTROLLER_DIR) && gofmt -w .
	@find runtime -name '*.cc' -o -name '*.h' | xargs clang-format -i

# -----------------------------------------------------
# Linting
# -----------------------------------------------------

lint:
	@echo "==> Linting Go code"
	@cd $(API_DIR) && go vet ./...
	@cd $(CONTROLLER_DIR) && go vet ./...

# -----------------------------------------------------
# Clean
# -----------------------------------------------------

clean:
	@echo "==> Cleaning build artifacts"
	@rm -rf $(BUILD_DIR)