# Generates C++ protobuf + gRPC sources for flow-pipe

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# --------------------------------------------------
# Paths
# --------------------------------------------------

set(PROTO_ROOT ${CMAKE_SOURCE_DIR}/proto)
set(PROTO_V1 ${PROTO_ROOT}/flowpipe/v1)
set(GOOGLEAPIS_DIR ${CMAKE_SOURCE_DIR}/third_party/googleapis)

# --------------------------------------------------
# Sanity check: googleapis
# --------------------------------------------------

if(NOT EXISTS ${GOOGLEAPIS_DIR}/google/api/annotations.proto)
    message(FATAL_ERROR
            "google/api/annotations.proto not found.\n"
            "Run:\n"
            "  git submodule update --init --recursive"
    )
endif()

# --------------------------------------------------
# Proto files (compile together)
# --------------------------------------------------

set(PROTO_FILES
        ${PROTO_V1}/flow.proto
        ${PROTO_V1}/service.proto
        ${GOOGLEAPIS_DIR}/google/api/annotations.proto
        ${GOOGLEAPIS_DIR}/google/api/http.proto
)

# --------------------------------------------------
# Generated outputs
# --------------------------------------------------

set(GENERATED_SRCS
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1/flow.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1/flow.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1/service.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1/service.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1/service.grpc.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1/service.grpc.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/google/api/annotations.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/google/api/annotations.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/google/api/http.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/google/api/http.pb.h
)

# Ensure output directories exist
file(MAKE_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}/flowpipe/v1
        ${CMAKE_CURRENT_BINARY_DIR}/google/api
)

# --------------------------------------------------
# Protobuf + gRPC generation
# --------------------------------------------------

add_custom_command(
        OUTPUT ${GENERATED_SRCS}
        COMMAND protoc
        ARGS
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        -I ${PROTO_ROOT}
        -I ${GOOGLEAPIS_DIR}
        -I ${Protobuf_INCLUDE_DIRS}
        ${PROTO_FILES}
        DEPENDS ${PROTO_FILES}
        COMMENT "Generating C++ protobuf and gRPC sources (flowpipe v1)"
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
        gRPC::grpc++
)

target_include_directories(flowpipe_proto
        PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)
