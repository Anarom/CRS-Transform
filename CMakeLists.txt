cmake_minimum_required(VERSION 3.28)

project(crs-transform VERSION 1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build tests" ON)

add_library(crs-transform)

add_subdirectory(include)
add_subdirectory(src)

find_package(PROJ REQUIRED)
target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC PROJ::proj)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
