# proto/CMakeLists.txt

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# --------------------------------------------------
# Proto roots
# --------------------------------------------------

set(PROTO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(GOOGLEAPIS_DIR ${CMAKE_SOURCE_DIR}/third_party/googleapis)

# --------------------------------------------------
# Proto files (compiled TOGETHER)
# --------------------------------------------------

set(PROTO_FILES
        ${PROTO_ROOT}/flow.proto
        ${PROTO_ROOT}/service.proto
        ${GOOGLEAPIS_DIR}/google/api/annotations.proto
        ${GOOGLEAPIS_DIR}/google/api/http.proto
)

# --------------------------------------------------
# Generated outputs
# --------------------------------------------------

set(GENERATED_SRCS
        ${CMAKE_CURRENT_BINARY_DIR}/flow.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/flow.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/service.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/service.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/service.grpc.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/service.grpc.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/google/api/annotations.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/google/api/annotations.pb.h
        ${CMAKE_CURRENT_BINARY_DIR}/google/api/http.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/google/api/http.pb.h
)

# Ensure output dirs exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/google/api)

# --------------------------------------------------
# Protobuf + gRPC code generation
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
        COMMENT "Generating C++ protobuf and gRPC sources"
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
