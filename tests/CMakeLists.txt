set(_tests_target_name "${PROJECT_NAME}-tests")

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(${_tests_target_name} CrsTransformationTests.cpp)

target_link_libraries(
    ${_tests_target_name}
    PRIVATE ${PROJECT_NAME} GTest::gtest_main)
gtest_discover_tests(${_tests_target_name})
