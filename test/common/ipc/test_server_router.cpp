//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "ipc/server_router.hpp"

#include "pipe/server_pipe_mock.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>

namespace
{

using namespace ocvsmd::common::ipc;  // NOLINT This our main concern here in the unit tests.

using testing::NotNull;
using testing::StrictMock;

class TestServerRouter : public testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// MARK: - Tests:

TEST_F(TestServerRouter, make)
{
    StrictMock<pipe::ServerPipeMock> server_pipe_mock;
    {
        auto       server_pipe   = std::make_unique<pipe::ServerPipeMock::RefWrapper>(server_pipe_mock);
        const auto server_router = ServerRouter::make(std::move(server_pipe));
        EXPECT_THAT(server_router, NotNull());

        EXPECT_CALL(server_pipe_mock, deinit()).Times(1);
    }
}

}  // namespace
