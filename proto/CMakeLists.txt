# Generates C++ protobuf sources for flow-pipe

find_package(Protobuf REQUIRED)

# --------------------------------------------------
# Paths
# --------------------------------------------------

set(PROTO_ROOT ${CMAKE_SOURCE_DIR}/proto)
set(PROTO_V1 ${PROTO_ROOT}/flowpipe/v1)

# --------------------------------------------------
# Proto files (compile together)
# --------------------------------------------------

set(PROTO_FILES
        ${PROTO_V1}/flow.proto
)

# --------------------------------------------------
# Generated outputs
# --------------------------------------------------

set(GENERATED_SRCS
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1/flow.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1/flow.pb.h
)

# Ensure output directories exist
file(MAKE_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1
)

# --------------------------------------------------
# Protobuf generation
# --------------------------------------------------

add_custom_command(
        OUTPUT ${GENERATED_SRCS}
        COMMAND protoc
        ARGS
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        -I ${PROTO_ROOT}
        -I ${Protobuf_INCLUDE_DIRS}
        ${PROTO_FILES}
        DEPENDS ${PROTO_FILES}
        COMMENT "Generating C++ protobuf (flowpipe v1)"
        VERBATIM
)

# --------------------------------------------------
# Library target
# --------------------------------------------------

add_library(flowpipe_proto
        ${GENERATED_SRCS}
)

target_link_libraries(flowpipe_proto
        PUBLIC
        protobuf::libprotobuf
)

target_include_directories(flowpipe_proto
        PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)
