cmake_minimum_required(VERSION 3.16)

project(tester)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)


find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(tester 
                  main.cpp 
                  logger.h
                  logger.cpp
              )

target_include_directories(tester PRIVATE CONAN_PKG::boost)
target_link_libraries(tester PRIVATE Threads::Threads CONAN_PKG::boost)