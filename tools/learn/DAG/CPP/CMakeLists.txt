# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(DAGExample VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler options
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find dependencies (optional)
find_package(fmt CONFIG QUIET)
if(fmt_FOUND)
    message(STATUS "Found fmt library")
else()
    message(STATUS "fmt not found, using standard output")
endif()

# Add executable
add_executable(dag_example examples.cpp)

# Link libraries
if(fmt_FOUND)
    target_link_libraries(dag_example fmt::fmt)
endif()

# Set compiler warnings
if(MSVC)
    target_compile_options(dag_example PRIVATE /W4 /WX)
else()
    target_compile_options(dag_example PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Enable address sanitizer in debug mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(dag_example PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(dag_example PRIVATE -fsanitize=address)
endif()

# Install target
install(TARGETS dag_example RUNTIME DESTINATION bin)
