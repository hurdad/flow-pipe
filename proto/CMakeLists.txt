# Generates C++ protobuf sources for flow-pipe

cmake_minimum_required(VERSION 3.20)

find_package(Protobuf REQUIRED)

# --------------------------------------------------
# Paths
# --------------------------------------------------

set(PROTO_ROOT ${CMAKE_SOURCE_DIR}/proto)
set(PROTO_V1 ${PROTO_ROOT}/flowpipe/v1)

# All generated files live here
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# --------------------------------------------------
# Proto files
# --------------------------------------------------

set(PROTO_FILES
        ${PROTO_V1}/flow.proto
        ${PROTO_V1}/observability.proto
)

# --------------------------------------------------
# Generated outputs
# --------------------------------------------------

set(GENERATED_SRCS
        ${PROTO_GEN_DIR}/flowpipe/v1/flow.pb.cc
        ${PROTO_GEN_DIR}/flowpipe/v1/flow.pb.h
)

file(MAKE_DIRECTORY ${PROTO_GEN_DIR}/flowpipe/v1)

# --------------------------------------------------
# Protobuf generation
# --------------------------------------------------

add_custom_command(
        OUTPUT ${GENERATED_SRCS}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS
        --cpp_out=${PROTO_GEN_DIR}
        -I ${PROTO_ROOT}
        ${PROTO_FILES}
        DEPENDS ${PROTO_FILES}
        COMMENT "Generating C++ protobuf (flowpipe v1)"
        VERBATIM
)

# --------------------------------------------------
# Library target
# --------------------------------------------------

add_library(flowpipe_proto STATIC
        ${GENERATED_SRCS}
)

target_link_libraries(flowpipe_proto
        PUBLIC protobuf::libprotobuf
)

# ✅ THIS IS THE CRITICAL FIX
target_include_directories(flowpipe_proto
        PUBLIC
        $<BUILD_INTERFACE:${PROTO_GEN_DIR}>
        $<INSTALL_INTERFACE:include>
)

# --------------------------------------------------
# Install rules
# --------------------------------------------------

install(TARGETS flowpipe_proto
        EXPORT flowpipeTargets
        ARCHIVE DESTINATION lib
)

install(
        DIRECTORY ${PROTO_GEN_DIR}/flowpipe/
        DESTINATION include/flowpipe
        FILES_MATCHING PATTERN "*.pb.h"
)
