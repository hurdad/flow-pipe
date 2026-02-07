# ============================================================
# flowpipe runtime
# ============================================================

# ------------------------------------------------------------
# Guard against double inclusion (VERY IMPORTANT)
# ------------------------------------------------------------
if(TARGET flowpipe_runtime)
    return()
endif()

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(Protobuf REQUIRED)
find_package(spdlog REQUIRED)
find_library(JEMALLOC_LIB
        NAMES jemalloc
        REQUIRED
)

# ------------------------------------------------------------
# Runtime shared library
# ------------------------------------------------------------
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

# ------------------------------------------------------------
# Runtime dependencies
# ------------------------------------------------------------
target_link_libraries(flowpipe_runtime
        PUBLIC
        protobuf::libprotobuf

        PRIVATE
        flowpipe_proto
        dl
        ${JEMALLOC_LIB}
        spdlog::spdlog
)

# ------------------------------------------------------------
# Optional OpenTelemetry
# ------------------------------------------------------------
if(FLOWPIPE_ENABLE_OTEL)
    target_link_libraries(flowpipe_runtime
            PRIVATE
            opentelemetry_metrics
            opentelemetry_trace
            opentelemetry_logs
            opentelemetry_resources

            opentelemetry_exporter_otlp_grpc
            opentelemetry_exporter_otlp_grpc_metrics
            opentelemetry_exporter_otlp_grpc_log

            opentelemetry_ext
            opentelemetry_exporter_otlp_http
            opentelemetry_exporter_otlp_http_metric
            opentelemetry_exporter_otlp_http_log
    )
endif()

target_compile_definitions(flowpipe_runtime
        PUBLIC $<$<BOOL:${FLOWPIPE_ENABLE_OTEL}>:FLOWPIPE_ENABLE_OTEL>
)

# ------------------------------------------------------------
# Headers
# ------------------------------------------------------------
target_include_directories(flowpipe_runtime
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ------------------------------------------------------------
# C++ standard & warnings
# ------------------------------------------------------------
target_compile_features(flowpipe_runtime
        PUBLIC cxx_std_20
)

target_compile_options(flowpipe_runtime
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

# ------------------------------------------------------------
# Runtime executable
# ------------------------------------------------------------
add_executable(flow_runtime
        cmd/flow_runtime/main.cc
)

target_link_libraries(flow_runtime
        PRIVATE
        flowpipe_runtime
        flowpipe_proto
        yaml-cpp
        spdlog::spdlog
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# ============================================================
# Install + CMake package config
# ============================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ------------------------------------------------------------
# Install runtime executable
# ------------------------------------------------------------
install(TARGETS flow_runtime
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ------------------------------------------------------------
# Install runtime shared library
# ------------------------------------------------------------
install(TARGETS flowpipe_runtime
        EXPORT flowpipeTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# ------------------------------------------------------------
# Install OpenTelemetry shared libraries (when enabled)
# ------------------------------------------------------------
if(FLOWPIPE_ENABLE_OTEL)
    install(TARGETS
            opentelemetry_common
            opentelemetry_api
            opentelemetry_sdk
            opentelemetry_metrics
            opentelemetry_trace
            opentelemetry_logs
            opentelemetry_resources
            opentelemetry_otlp_recordable
            opentelemetry_exporter_otlp_grpc
            opentelemetry_exporter_otlp_grpc_metrics
            opentelemetry_exporter_otlp_grpc_log
            opentelemetry_exporter_otlp_grpc_client
            opentelemetry_ext
            opentelemetry_exporter_otlp_http
            opentelemetry_exporter_otlp_http_metric
            opentelemetry_exporter_otlp_http_log
            opentelemetry_exporter_otlp_http_client
            opentelemetry_http_client_curl
            opentelemetry_proto
            EXPORT flowpipeTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# ------------------------------------------------------------
# Install public headers
# ------------------------------------------------------------
install(
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# ------------------------------------------------------------
# Export targets
# ------------------------------------------------------------
install(EXPORT flowpipeTargets
        NAMESPACE flowpipe::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flowpipe
)

# ------------------------------------------------------------
# Generate package config files
# ------------------------------------------------------------
configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/flowpipeConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipeConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flowpipe
)

write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipeConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

# ------------------------------------------------------------
# Install package config files
# ------------------------------------------------------------
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipeConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipeConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flowpipe
)
