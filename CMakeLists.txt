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

add_subdirectory(src)
add_subdirectory(test)
