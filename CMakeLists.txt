#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

project(ocvsmd
        VERSION ${OCVSMD_VERSION}
        LANGUAGES CXX
        HOMEPAGE_URL https://github.com/OpenCyphal-Garage/opencyphal-vehicle-system-management-daemon
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set the output binary directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(src_dir "${CMAKE_SOURCE_DIR}/src")
set(test_dir "${CMAKE_SOURCE_DIR}/test")
set(include_dir "${CMAKE_SOURCE_DIR}/include")

# clang-format
find_program(clang_format NAMES clang-format)
if (NOT clang_format)
    message(STATUS "Could not locate clang-format")
else ()
    file(GLOB format_files ${include_dir}/**/*.[ch]pp ${src_dir}/**/*.[ch]pp ${test_dir}/**/*.[ch]pp)
    message(STATUS "Using clang-format: ${clang_format}; files: ${format_files}")
    add_custom_target(format COMMAND ${clang_format} -i -fallback-style=none -style=file --verbose ${format_files})
endif ()

add_subdirectory(src)
add_subdirectory(test)
