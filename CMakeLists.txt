cmake_minimum_required(VERSION 3.16)

project(tiny_vfs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tiny_vfs INTERFACE)
target_include_directories(tiny_vfs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(tiny_vfs INTERFACE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
    message(FATAL_ERROR "GCC 7+ is required for C++17 support.")
endif()

add_executable(tiny_vfs_test tests/tiny_vfs_test.cpp)
target_link_libraries(tiny_vfs_test PRIVATE tiny_vfs)

add_executable(tiny_vfs_example examples/example_vfs.cpp)
target_link_libraries(tiny_vfs_example PRIVATE tiny_vfs)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" TINYVFS_SOURCE_DIR)
target_compile_definitions(tiny_vfs_example PRIVATE TINYVFS_SOURCE_DIR="${TINYVFS_SOURCE_DIR}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
    target_link_libraries(tiny_vfs_test PRIVATE stdc++fs)
    target_link_libraries(tiny_vfs_example PRIVATE stdc++fs)
endif()

enable_testing()
add_test(NAME tiny_vfs_test COMMAND tiny_vfs_test)
