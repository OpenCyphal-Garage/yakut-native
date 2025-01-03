#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.27)

add_executable(engine_tests
        test_xxx.cpp
)
target_link_libraries(engine_tests
        ocvsmd_engine
        GTest::gmock
        GTest::gtest_main
)

gtest_discover_tests(engine_tests)
