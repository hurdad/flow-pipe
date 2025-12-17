# --------------------------------------------------
# Runtime executable
# --------------------------------------------------

add_executable(flowpipe-runtime
        src/main.cc

        # Pipeline core
        src/pipeline/pipeline_builder.cc
        src/pipeline/stage_registry.cc

        # Add more source files here as the runtime grows
)

# --------------------------------------------------
# Include paths
# --------------------------------------------------

target_include_directories(flowpipe-runtime
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --------------------------------------------------
# Link dependencies
# --------------------------------------------------

target_link_libraries(flowpipe-runtime
        PRIVATE
        flowpipe_proto
)

# --------------------------------------------------
# Optional: pthread (Linux)
# --------------------------------------------------

find_package(Threads REQUIRED)
target_link_libraries(flowpipe-runtime PRIVATE Threads::Threads)
