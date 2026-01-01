cmake_minimum_required(VERSION 3.20)

project(flowpipe_runtime
        VERSION 0.1.0
        LANGUAGES CXX
)

# -------------------------------------------------
# Global RPATH (plugins live in ../plugins, runtime in ../lib)
# -------------------------------------------------
set(CMAKE_BUILD_RPATH "$ORIGIN/../lib")
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# -------------------------------------------------
# Dependencies
# -------------------------------------------------
find_package(Protobuf REQUIRED)
find_package(spdlog REQUIRED)
find_library(JEMALLOC_LIB
        NAMES jemalloc
        REQUIRED
)

# =================================================
# Runtime shared library
# =================================================
add_library(flowpipe_runtime SHARED
        src/runtime.cc
        src/signal_handler.cc

        # Plugin infrastructure
        src/stage_factory.cc
        src/stage_registry.cc

        # Stage execution
        src/stage_metrics.cc
        src/stage_runner.cc

        # Observability
        src/observability/defaults.cc
        src/observability/observability.cc
        src/observability/observability_state.cc
        src/observability/metrics.cc
        src/observability/tracing.cc
        src/observability/logging.cc
        src/observability/logging_runtime.cc
        src/observability/local_logging.cc
)

# -------------------------------------------------
# Runtime dependencies
# -------------------------------------------------
target_link_libraries(flowpipe_runtime
        PUBLIC
        protobuf::libprotobuf

        PRIVATE
        flowpipe_proto
        dl
        ${JEMALLOC_LIB}
        spdlog::spdlog
)

# -------------------------------------------------
# Optional OpenTelemetry
# -------------------------------------------------
if(FLOWPIPE_ENABLE_OTEL)
    target_link_libraries(flowpipe_runtime
            PRIVATE
            opentelemetry_metrics
            opentelemetry_trace
            opentelemetry_logs
            opentelemetry_resources

            # gRPC exporters
            opentelemetry_exporter_otlp_grpc
            opentelemetry_exporter_otlp_grpc_metrics
            opentelemetry_exporter_otlp_grpc_log

            # HTTP exporters
            opentelemetry_ext
            opentelemetry_exporter_otlp_http
            opentelemetry_exporter_otlp_http_metric
            opentelemetry_exporter_otlp_http_log
    )
endif()

target_compile_definitions(flowpipe_runtime
        PUBLIC $<$<BOOL:${FLOWPIPE_ENABLE_OTEL}>:FLOWPIPE_ENABLE_OTEL>
)

# -------------------------------------------------
# Headers
# -------------------------------------------------
target_include_directories(flowpipe_runtime
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# -------------------------------------------------
# C++ standard + warnings
# -------------------------------------------------
target_compile_features(flowpipe_runtime
        PUBLIC cxx_std_20
)

target_compile_options(flowpipe_runtime
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# =================================================
# Runtime executable
# =================================================
add_executable(flow_runtime
        cmd/flow_runtime/main.cc
)

target_link_libraries(flow_runtime
        PUBLIC
        flowpipe_runtime
        PRIVATE
        flowpipe_proto
        yaml-cpp
)

# =================================================
# Install rules
# =================================================
include(GNUInstallDirs)

# Runtime executable
install(TARGETS flow_runtime
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Runtime shared library
install(TARGETS flowpipe_runtime
        EXPORT flowpipeTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Public headers
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# CMake package export (for in-tree stages)
install(EXPORT flowpipeTargets
        NAMESPACE flowpipe::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flowpipe
)
