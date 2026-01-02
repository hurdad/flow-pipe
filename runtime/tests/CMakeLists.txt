cmake_minimum_required(VERSION 3.22)

enable_testing()

# Prefer a system-installed GTest but fall back to FetchContent if unavailable.
find_package(GTest QUIET)
if(GTest_FOUND)
    set(GTEST_MAIN_TARGET GTest::gtest_main)
else()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    # For Windows builds inside FetchContent
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set(GTEST_MAIN_TARGET gtest_main)
endif()

include(GoogleTest)

add_executable(stage_registry_test
    stage_registry_test.cc
)
target_link_libraries(stage_registry_test
    PRIVATE
        flowpipe_runtime
        ${GTEST_MAIN_TARGET}
)
gtest_discover_tests(stage_registry_test)

add_executable(stage_runner_test
    stage_runner_test.cc
)
target_link_libraries(stage_runner_test
    PRIVATE
        flowpipe_runtime
        ${GTEST_MAIN_TARGET}
)
gtest_discover_tests(stage_runner_test)
