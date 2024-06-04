cmake_minimum_required(VERSION 3.10)

project(WEBV2)
set(CMAKE_CXX_STANDARD 11)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(test ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/test_log.cpp
${CMAKE_CURRENT_SOURCE_DIR}/src/test_threadpool.cpp)