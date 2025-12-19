cmake_minimum_required(VERSION 3.20)

project(stage_noop_transform LANGUAGES CXX)

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
)

target_compile_features(stage_noop_transform
        PRIVATE cxx_std_20
)
