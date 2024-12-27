#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.27)

set(dsdl_ocvsmd_dir ${CMAKE_CURRENT_SOURCE_DIR}/dsdl/ocvsmd)
file(GLOB_RECURSE dsdl_ocvsmd_files CONFIGURE_DEPENDS ${dsdl_ocvsmd_dir}/*.dsdl)

add_cyphal_library(
        NAME common
        DSDL_FILES ${dsdl_ocvsmd_files}
        DSDL_NAMESPACES ${dsdl_ocvsmd_dir}
        ALLOW_EXPERIMENTAL_LANGUAGES
        LANGUAGE cpp
        LANGUAGE_STANDARD cetl++14-17
        OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/dsdl_transpiled
        OUT_LIBRARY_TARGET common_transpiled
)

add_library(ocvsmd_common
        ipc/unix_socket_client.cpp
        ipc/unix_socket_server.cpp
)
target_link_libraries(ocvsmd_common
        PUBLIC ${common_transpiled}
)
target_include_directories(ocvsmd_common
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(ocvsmd_common SYSTEM
        PUBLIC ${submodules_dir}/cetl/include
        PUBLIC ${submodules_dir}/libcyphal/include
)
add_dependencies(ocvsmd_common
        ${common_transpiled}
)
