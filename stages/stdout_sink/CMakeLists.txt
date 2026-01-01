cmake_minimum_required(VERSION 3.20)

project(stage_stdout_sink LANGUAGES CXX)

add_library(stage_stdout_sink SHARED
        stdout_sink.cc
)

set_target_properties(stage_stdout_sink PROPERTIES
        OUTPUT_NAME stage_stdout_sink
        PREFIX "lib"
)

target_include_directories(stage_stdout_sink
        PRIVATE
        ${PROJECT_SOURCE_DIR}/../../runtime/include
)

target_link_libraries(stage_stdout_sink
        PRIVATE
        flowpipe_runtime
)

target_compile_features(stage_stdout_sink
        PRIVATE cxx_std_20
)
