cmake_minimum_required(VERSION 3.10)

project(WEBV2)
set(CMAKE_CXX_STANDARD 11)


add_executable(test ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(/usr/include/mysql)
target_link_libraries(test /usr/lib/x86_64-linux-gnu/libmysqlclient.so)