#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.27)

add_executable(common_tests
        ipc/test_client_router.cpp
        ipc/test_server_router.cpp
)
target_link_libraries(common_tests
        ocvsmd_common
        GTest::gmock
        GTest::gtest_main
)

gtest_discover_tests(common_tests)
