cmake_minimum_required(VERSION 3.10)
project(sen-inference)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
# Set output directory for executable
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -DDEBUG -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Compiler Options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++17 -Wall -Wextra -Werror")

# Find PEGTL (assuming it's installed in /boot/home/config/non-packaged)
find_path(PEGTL_INCLUDE_DIR tao/pegtl.hpp
    PATHS /boot/home/config/non-packaged/include
)
if(NOT PEGTL_INCLUDE_DIR)
    message(FATAL_ERROR "TAO PEGTL not found")
endif()

include_directories(${PEGTL_INCLUDE_DIR} src)

add_executable(sen-inference
    src/main.cpp
    src/inference_engine.cpp
)

target_include_directories(sen-inference PRIVATE src)
