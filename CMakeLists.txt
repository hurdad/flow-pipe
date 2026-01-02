cmake_minimum_required(VERSION 3.22)

project(flow_pipe
        VERSION 0.1.0
        DESCRIPTION "Kubernetes-native pipeline runtime"
        LANGUAGES CXX
)

# --------------------------------------------------
# Global C++ configuration
# --------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Position-independent code (shared libs, plugins, etc.)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Export compile_commands.json for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(CTest)

# --------------------------------------------------
# Compiler warnings
# --------------------------------------------------

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# --------------------------------------------------
# Optional components
# --------------------------------------------------

option(FLOWPIPE_ENABLE_OTEL "Enable OpenTelemetry integration" OFF)
add_compile_definitions($<$<BOOL:${FLOWPIPE_ENABLE_OTEL}>:FLOWPIPE_ENABLE_OTEL>)

if(FLOWPIPE_ENABLE_OTEL)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/opentelemetry-cpp/CMakeLists.txt)
        message(FATAL_ERROR "FLOWPIPE_ENABLE_OTEL is enabled but the opentelemetry-cpp submodule is missing. "
                "Run 'git submodule update --init --recursive' to fetch third_party/opentelemetry-cpp.")
    endif()

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Dont build Shared Libs" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "Disable tests for dependencies" FORCE)
    set(WITH_OTLP_GRPC ON CACHE BOOL "Build OTLP gRPC exporter" FORCE)
    set(WITH_OTLP_HTTP ON CACHE BOOL "Build OTLP HTTP exporter" FORCE)

    add_subdirectory(third_party/opentelemetry-cpp)
endif()

# --------------------------------------------------
# Subdirectories
# --------------------------------------------------

add_subdirectory(proto)
add_subdirectory(runtime)
add_subdirectory(stages/noop_source)
add_subdirectory(stages/noop_transform)
add_subdirectory(stages/stdout_sink)
