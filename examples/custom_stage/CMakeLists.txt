add_executable(flowpipe_example_custom_stage
        main.cc
        uppercase_stage.cc
)

target_link_libraries(flowpipe_example_custom_stage
        PRIVATE flowpipe_runtime
)

target_include_directories(flowpipe_example_custom_stage
        PRIVATE ${PROJECT_SOURCE_DIR}/runtime/include
)
