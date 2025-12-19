# =====================================================
# flow-pipe Makefile
# =====================================================

# -----------------------------------------------------
# Directories
# -----------------------------------------------------

BUILD_DIR        := cmake-build
PROTO_GO_SCRIPT  := tools/gen_go_protos.sh

API_DIR         := api
CONTROLLER_DIR  := controller
RUNTIME_DIR     := runtime

# -----------------------------------------------------
# Tools
# -----------------------------------------------------

CMAKE ?= cmake
GO    ?= go

# -----------------------------------------------------
# Phony targets
# -----------------------------------------------------

.PHONY: \
	all \
	submodules \
	configure \
	proto proto-go proto-cpp \
	runtime \
	api controller \
	format fmt \
	lint \
	install \
	clean help

# -----------------------------------------------------
# Default target
# -----------------------------------------------------

all: submodules proto runtime api controller

# -----------------------------------------------------
# Help
# -----------------------------------------------------

help:
	@echo "flow-pipe build targets:"
	@echo ""
	@echo "  all           Init submodules, build everything"
	@echo "  proto         Generate Go + C++ protobufs"
	@echo "  runtime       Build C++ runtime"
	@echo "  api           Build Go API"
	@echo "  controller    Build Go controller"
	@echo "  format | fmt  Format Go and C++ code"
	@echo "  lint          Lint Go code"
	@echo "  install       Install C++ runtime"
	@echo "  clean         Remove build artifacts"
	@echo ""

# -----------------------------------------------------
# Submodules
# -----------------------------------------------------

submodules:
	@echo "==> Updating git submodules"
	@git submodule update --init --recursive

# -----------------------------------------------------
# CMake configure (one-time)
# -----------------------------------------------------

configure:
	@echo "==> Configuring C++ build"
	@$(CMAKE) -S . -B $(BUILD_DIR)

# -----------------------------------------------------
# Protobuf
# -----------------------------------------------------

proto: proto-go proto-cpp

proto-go:
	@echo "==> Generating Go protobufs"
	@$(PROTO_GO_SCRIPT)

proto-cpp: configure
	@echo "==> Generating C++ protobufs"
	@$(CMAKE) --build $(BUILD_DIR) --target flowpipe_proto

# -----------------------------------------------------
# Runtime (C++)
# -----------------------------------------------------

runtime: configure
	@echo "==> Building C++ runtime"
	@$(CMAKE) --build $(BUILD_DIR)

install: configure
	@echo "==> Installing C++ runtime"
	@$(CMAKE) --install $(BUILD_DIR)

# -----------------------------------------------------
# Go API
# -----------------------------------------------------

api: proto
	@echo "==> Building Flow API"
	@cd $(API_DIR) && $(GO) build ./...

# -----------------------------------------------------
# Go Controller
# -----------------------------------------------------

controller: proto
	@echo "==> Building Flow Controller"
	@cd $(CONTROLLER_DIR) && $(GO) build ./...

# -----------------------------------------------------
# Formatting
# -----------------------------------------------------

format:
	@echo "==> Formatting Go code"
	@cd $(API_DIR) && gofmt -w .
	@cd $(CONTROLLER_DIR) && gofmt -w .

	@echo "==> Formatting C++ code (Google style)"
	@find $(RUNTIME_DIR) \
		\( -name '*.cc' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
		-print \
		| xargs clang-format -i

# Alias
fmt: format

# -----------------------------------------------------
# Linting
# -----------------------------------------------------

lint:
	@echo "==> Linting Go code"
	@cd $(API_DIR) && $(GO) vet ./...
	@cd $(CONTROLLER_DIR) && $(GO) vet ./...

# -----------------------------------------------------
# Clean
# -----------------------------------------------------

clean:
	@echo "==> Cleaning build artifacts"
	@rm -rf $(BUILD_DIR)
