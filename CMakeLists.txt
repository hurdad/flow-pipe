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
# Subdirectories
# --------------------------------------------------

add_subdirectory(proto)
add_subdirectory(runtime)
add_subdirectory(stages/noop_source)
add_subdirectory(stages/noop_transform)
add_subdirectory(stages/stdout_sink)