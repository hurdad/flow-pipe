cmake_minimum_required(VERSION 3.20)

project(flowpipe_runtime
        VERSION 0.1.0
        LANGUAGES CXX
)

# =========================
# Library target
# =========================
add_library(flowpipe_runtime
        src/runtime.cc
        src/signal_handler.cc
        src/stages/noop_source.cc
        src/stages/noop_transform.cc
        src/stages/stdout_sink.cc
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
        PRIVATE flowpipe_runtime
)

# =========================
# Install rules
# =========================
include(GNUInstallDirs)

install(
        TARGETS flowpipe_runtime flow_runtime
        EXPORT flowpipeRuntimeTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
        DIRECTORY include/flowpipe
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# =========================
# Export targets
# =========================
install(
        EXPORT flowpipeRuntimeTargets
        NAMESPACE flowpipe::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flowpipe
)
