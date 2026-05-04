cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

project(stdio_VM_runner)

# Platform configuration
if(WIN32)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /MP /Zc:rvalueCast")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
    # different release and debug flags
	set(CMAKE_CXX_FLAGS_RELEASE "-O3")
	set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer")
endif()

add_executable(stdio_VM_runner stdio_VM_runner.cpp)

