cmake_minimum_required(VERSION 3.16)
project(server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)


find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

add_executable(server 
                server.cpp 
                server_interface.cpp 
                server_interface.h 
                server_routines.cpp 
                server_routines.h
                logger.h
                logger.cpp
                )

target_include_directories(server PRIVATE CONAN_PKG::boost)
target_link_libraries(server pthread CONAN_PKG::boost)
