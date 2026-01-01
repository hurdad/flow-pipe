cmake_minimum_required(VERSION 3.20)

project(stage_noop_source LANGUAGES CXX)

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
)

target_compile_features(stage_noop_source
        PRIVATE cxx_std_20
)
