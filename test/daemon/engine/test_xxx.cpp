//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

class TestXxx : public testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// MARK: - Tests:

TEST_F(TestXxx, make)
{
    // Expect two strings not to be equal.
    EXPECT_STRNE("hello", "world");
    // Expect equality.
    EXPECT_THAT(7 * 6, 42);
}

}  // namespace
