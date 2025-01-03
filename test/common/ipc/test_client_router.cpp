//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "ipc/client_router.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{

using namespace ocvsmd::common::ipc;  // NOLINT This our main concern here in the unit tests.

using testing::NotNull;

class TestClientRouter : public testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// MARK: - Tests:

TEST_F(TestClientRouter, make)
{
    const auto router = ClientRouter::make();
    EXPECT_THAT(router, NotNull());
}

}  // namespace
