cmake_minimum_required(VERSION 3.20)

project(flowpipe_runtime
        VERSION 0.1.0
        LANGUAGES CXX
)

# =========================
# Runtime library
# =========================
add_library(flowpipe_runtime
        src/runtime.cc
        src/metrics/otel_metrics.cc
        src/otel/telemetry.cc
        src/signal_handler.cc

        # Plugin infrastructure
        src/stage_factory.cc
        src/stage_registry.cc
)

# Runtime depends on:
# - flowpipe_proto (for FlowSpec, StageSpec, etc.)
# - dl (for dlopen/dlsym)
target_link_libraries(flowpipe_runtime
        PRIVATE
        flowpipe_proto
        dl
)

if(FLOWPIPE_ENABLE_OTEL)
    target_link_libraries(flowpipe_runtime
            PRIVATE
            opentelemetry_metrics
            opentelemetry_trace
            opentelemetry_logs
            opentelemetry_resources
            opentelemetry_exporter_otlp_grpc
    )
endif()

target_compile_definitions(flowpipe_runtime
        PUBLIC $<$<BOOL:${FLOWPIPE_ENABLE_OTEL}>:FLOWPIPE_ENABLE_OTEL>
)

# Public headers (build vs install)
target_include_directories(flowpipe_runtime
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# C++ standard
target_compile_features(flowpipe_runtime
        PUBLIC cxx_std_20
)

# Warnings (local to runtime only)
target_compile_options(flowpipe_runtime
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# =========================
# Runtime executable
# =========================
add_executable(flow_runtime
        cmd/flow_runtime/main.cc
)

target_link_libraries(flow_runtime
        PUBLIC flowpipe_runtime
        PRIVATE flowpipe_proto yaml-cpp
)

# =========================
# Install rules
# =========================

include(GNUInstallDirs)

# ---- Runtime library ----
#install(TARGETS flowpipe_runtime
#        EXPORT flowpipeTargets
#        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#)

# ---- Runtime executable ----
install(TARGETS flow_runtime
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---- Public headers ----
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ---- Export targets for downstream consumers ----
install(EXPORT flowpipeTargets
        NAMESPACE flowpipe::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flowpipe
)
