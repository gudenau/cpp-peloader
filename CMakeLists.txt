cmake_minimum_required(VERSION 3.26.4)

project(PeLoader VERSION 0.0.0 DESCRIPTION "Loading DLLs on Linux")

# All projects

set(CMAKE_C_STANDARD 17)

set(CMAKE_C_COMPILER clang)
set(CMAKE_LINKER clang)

set(CMAKE_C_FLAGS "-fvisibility=hidden -Wall -Wextra -Werror")
set(CMAKE_LINKER_FLAGS "-fuse-ld=lld -fvisibility=hidden")

add_compile_definitions(__peloader_build__)

# PeLoader

add_library(PeLoader SHARED
    public/peloader.h

    include/internal.h
    include/io.h
    include/pefile.h

    source/io.cpp
    source/PeLoader.cpp
)

target_include_directories(PeLoader PRIVATE include)
target_include_directories(PeLoader PUBLIC public)

# Test program

add_executable(PeLoaderTest
    test/include/testLib.h

    test/main.cpp
)

target_include_directories(PeLoader PRIVATE test/include)

target_link_libraries(PeLoaderTest PeLoader)
