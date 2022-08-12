cmake_minimum_required(VERSION 3.0.0)
project(server)

set(CMAKE _CXX_STANDARD 17)

add_executable(server server.cpp server_interface.cpp server_interface.h)
target_link_libraries(server -lpthread)
