cmake_minimum_required(VERSION 3.16)

project(server_async)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

#set(CMAKE_CXX_FLAGS "-fcoroutines -fconcepts")


find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(async_server 
                    main.cpp 
                    server.cpp 
                    server.h
                    logger.h
                    logger.cpp
                    )

target_include_directories(async_server PRIVATE CONAN_PKG::boost)
target_link_libraries(async_server PRIVATE Threads::Threads CONAN_PKG::boost)