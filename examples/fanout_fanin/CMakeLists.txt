add_executable(flowpipe_example_fanout_fanin
        main.cc
        fanout_stage.cc
        aggregate_stage.cc
)

target_link_libraries(flowpipe_example_fanout_fanin
        PRIVATE flowpipe_runtime
)

target_include_directories(flowpipe_example_fanout_fanin
        PRIVATE ${PROJECT_SOURCE_DIR}/runtime/include
)
