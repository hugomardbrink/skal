cmake_minimum_required(VERSION 3.21)

project(skal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add everything in src/ to the executable
file(GLOB_RECURSE SOURCES src/*.cxx src/*.hxx)
add_executable(skal ${SOURCES})

target_link_libraries(skal PRIVATE readline)

file(GLOB_RECURSE TEST_SOURCES test/*.cxx test/*.hxx src/shell.*xx src/parser.*xx)
add_executable(test ${TEST_SOURCES})

target_link_libraries(test PRIVATE readline)
