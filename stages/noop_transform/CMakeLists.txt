cmake_minimum_required(VERSION 3.20)

project(stage_noop_transform LANGUAGES CXX)

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(Protobuf REQUIRED)

# ------------------------------------------------------------
# Proto
# ------------------------------------------------------------
set(PROTO_FILES
        noop_transform.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(stage_noop_proto STATIC
        ${PROTO_SRCS}
        ${PROTO_HDRS}
)

target_link_libraries(stage_noop_proto
        PUBLIC protobuf::libprotobuf
)

target_include_directories(stage_noop_proto
        PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_features(stage_noop_proto
        PUBLIC cxx_std_20
)

# ------------------------------------------------------------
# Stage plugin
# ------------------------------------------------------------
add_library(stage_noop_transform SHARED
        noop_transform.cc
)

set_target_properties(stage_noop_transform PROPERTIES
        OUTPUT_NAME stage_noop_transform
        PREFIX "lib"
)

target_include_directories(stage_noop_transform
        PRIVATE
        ${PROJECT_SOURCE_DIR}/../../runtime/include
        ${CMAKE_CURRENT_BINARY_DIR}   # for generated .pb.h
)

target_link_libraries(stage_noop_transform
        PRIVATE
        flowpipe_runtime
        stage_noop_proto
)

target_compile_features(stage_noop_transform
        PRIVATE cxx_std_20
)

install(TARGETS stage_noop_transform
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
)
# ------------------------------------------------------------
# Plugin hardening (recommended)
# ------------------------------------------------------------
target_compile_options(stage_noop_transform PRIVATE
        -Wall -Wextra -Wpedantic
)

set_target_properties(stage_noop_transform PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
)
