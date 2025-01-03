//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "client_pipe_mock.hpp"
#include "ipc/client_router.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace
{

using namespace ocvsmd::common::ipc;  // NOLINT This our main concern here in the unit tests.

using testing::NotNull;
using testing::StrictMock;

class TestClientRouter : public testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// MARK: - Tests:

TEST_F(TestClientRouter, make)
{
    StrictMock<ClientPipeMock> client_pipe_mock;
    {
        auto       client_pipe   = std::make_unique<ClientPipeMock::RefWrapper>(client_pipe_mock);
        const auto client_router = ClientRouter::make(std::move(client_pipe));
        EXPECT_THAT(client_router, NotNull());

        EXPECT_CALL(client_pipe_mock, deinit()).Times(1);
    }
}

}  // namespace
