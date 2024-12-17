#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.25)

add_library(ocvsmd_engine
        application.cpp
        config.cpp
)
target_include_directories(ocvsmd_engine
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        PUBLIC ${submodules}/cetl/include
        PUBLIC ${submodules}/libcyphal/include
)
