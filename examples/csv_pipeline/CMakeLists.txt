add_executable(flowpipe_example_csv_pipeline
        main.cc
        csv_source.cc
        csv_parser.cc
        csv_sink.cc
)

target_link_libraries(flowpipe_example_csv_pipeline
        PRIVATE flowpipe_runtime flowpipe_proto
)

target_include_directories(flowpipe_example_csv_pipeline
        PRIVATE ${PROJECT_SOURCE_DIR}/runtime/include
)
