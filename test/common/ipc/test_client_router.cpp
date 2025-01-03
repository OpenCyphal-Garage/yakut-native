//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "ipc/client_router.hpp"

#include "pipe/client_pipe_mock.hpp"

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
    StrictMock<pipe::ClientPipeMock> client_pipe_mock;
    {
        auto       client_pipe   = std::make_unique<pipe::ClientPipeMock::RefWrapper>(client_pipe_mock);
        const auto client_router = ClientRouter::make(std::move(client_pipe));
        EXPECT_THAT(client_router, NotNull());

        EXPECT_CALL(client_pipe_mock, deinit()).Times(1);
    }
}

TEST_F(TestClientRouter, makeChannel)
{
    StrictMock<pipe::ClientPipeMock> client_pipe_mock;
    {
        auto       client_pipe   = std::make_unique<pipe::ClientPipeMock::RefWrapper>(client_pipe_mock);
        const auto client_router = ClientRouter::make(std::move(client_pipe));
        EXPECT_THAT(client_router, NotNull());

        using Data = cetl::span<const std::uint8_t>;

        auto ch = client_router->makeChannel<Data, Data>([](const auto&) {});
        //
        // const std::array<std::uint8_t, 4> data{1, 2, 3, 4};
        // ch.send(data);

        EXPECT_CALL(client_pipe_mock, deinit()).Times(1);
    }
}

}  // namespace
