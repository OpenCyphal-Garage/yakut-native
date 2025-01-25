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
        LANGUAGE_STANDARD ${CYPHAL_LANGUAGE_STANDARD}
        OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/dsdl_transpiled
        OUT_LIBRARY_TARGET common_transpiled
)

add_library(ocvsmd_common
        ipc/client_router.cpp
        ipc/pipe/net_socket_client.cpp
        ipc/pipe/net_socket_server.cpp
        ipc/pipe/socket_base.cpp
        ipc/pipe/socket_server_base.cpp
        ipc/pipe/unix_socket_client.cpp
        ipc/pipe/unix_socket_server.cpp
        ipc/server_router.cpp
)
target_link_libraries(ocvsmd_common
        PUBLIC ${common_transpiled}
)
target_include_directories(ocvsmd_common
        PUBLIC ${include_dir}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(ocvsmd_common SYSTEM
        PUBLIC ${submodules_dir}/cetl/include
        PUBLIC ${submodules_dir}/libcyphal/include
)
add_dependencies(ocvsmd_common
        ${common_transpiled}
)
