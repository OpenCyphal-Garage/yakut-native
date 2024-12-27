#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.27)

# Define type generation and header library all in one go.
#
set(dsdl_types_in_engine # List all the DSDL types used in the engine
        uavcan/node/430.GetInfo.1.0.dsdl
        uavcan/node/435.ExecuteCommand.1.3.dsdl
        uavcan/node/7509.Heartbeat.1.0.dsdl
        uavcan/register/384.Access.1.0.dsdl
        uavcan/register/385.List.1.0.dsdl
)
add_cyphal_library(
        NAME engine
        DSDL_FILES ${dsdl_types_in_engine}
        ALLOW_EXPERIMENTAL_LANGUAGES
        LANGUAGE cpp
        LANGUAGE_STANDARD cetl++14-17
        OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/dsdl_transpiled
        OUT_LIBRARY_TARGET engine_transpiled
)

add_library(udpard
        ${submodules_dir}/libudpard/libudpard/udpard.c
)
target_include_directories(udpard
        INTERFACE SYSTEM ${submodules_dir}/libudpard/libudpard
)

add_library(ocvsmd_engine
        application.cpp
        config.cpp
        platform/udp/udp.c
)
target_link_libraries(ocvsmd_engine
        PUBLIC udpard
        PUBLIC ${engine_transpiled}
        PUBLIC ocvsmd_common
)
target_include_directories(ocvsmd_engine
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_include_directories(ocvsmd_engine SYSTEM
        PUBLIC ${submodules_dir}/cetl/include
        PUBLIC ${submodules_dir}/libcyphal/include
)
add_dependencies(ocvsmd_engine
        ${engine_transpiled}
)
