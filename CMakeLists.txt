cmake_minimum_required(VERSION 3.11)

project(sen-inference LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -DDEBUG -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Compiler Options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++17 -Wall -Wextra -Werror")

set(SOURCE  src/main.cpp
            src/inference_engine.cpp
    )

set(HEADERS src/inference_engine.h
    )

include_directories(${LB_LIBS_INCLUDE_DIRS} /boot/home/config/non-packaged/include)

add_executable(sen-inference ${SOURCE} ${HEADERS})

set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 17
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(${PROJECT_NAME} be localestub translation)
