cmake_minimum_required(VERSION 3.11)

project(sen-inference LANGUAGES CXX)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# Compiler Options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++17 -Wall -Wextra -Werror")

set(SOURCE  src/main.cpp
    )

set(HEADERS src/inference_engine.h
    )

include_directories(/boot/home/config/non-packaged/include)

add_executable(sen-inference ${SOURCE} ${HEADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 17
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
