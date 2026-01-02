cmake_minimum_required(VERSION 3.20)

project(stage_noop_source LANGUAGES CXX)

# ------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------
find_package(Protobuf REQUIRED)

# ------------------------------------------------------------
# Proto
# ------------------------------------------------------------
set(PROTO_FILES
        noop_source.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(stage_noop_source_proto STATIC
        ${PROTO_SRCS}
        ${PROTO_HDRS}
)

target_link_libraries(stage_noop_source_proto
        PUBLIC protobuf::libprotobuf
)

target_include_directories(stage_noop_source_proto
        PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_features(stage_noop_source_proto
        PUBLIC cxx_std_20
)

# ------------------------------------------------------------
# Stage plugin
# ------------------------------------------------------------
add_library(stage_noop_source SHARED
        noop_source.cc
)

set_target_properties(stage_noop_source PROPERTIES
        OUTPUT_NAME stage_noop_source
        PREFIX "lib"
)

target_include_directories(stage_noop_source
        PRIVATE
        ${PROJECT_SOURCE_DIR}/../../runtime/include
)

target_link_libraries(stage_noop_source
        PRIVATE
        flowpipe_runtime
        stage_noop_source_proto
)

target_compile_features(stage_noop_source
        PRIVATE cxx_std_20
)

install(TARGETS stage_noop_source
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
)