//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "ipc/server_router.hpp"

#include "ipc/channel.hpp"
#include "ipc/pipe/server_pipe.hpp"
#include "pipe/server_pipe_mock.hpp"
#include "tracking_memory_resource.hpp"

#include "ocvsmd/common/ipc/Route_1_0.hpp"
#include "ocvsmd/common/node_command/ExecCmd_1_0.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace
{

using namespace ocvsmd::common::ipc;  // NOLINT This our main concern here in the unit tests.

using testing::_;
using testing::IsTrue;
using testing::IsEmpty;
using testing::IsFalse;
using testing::NotNull;
using testing::StrictMock;
using testing::VariantWith;
using testing::MockFunction;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestServerRouter : public testing::Test
{
protected:
    void SetUp() override
    {
        cetl::pmr::set_default_resource(&mr_);
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    template <typename Message, typename Action>
    void withRouteChannelMsg(const cetl::string_view service_name,
                             const std::uint64_t     tag,
                             const Message&          message,
                             Action                  action)
    {
        using ocvsmd::common::tryPerformOnSerialized;

        Route_1_0 route{&mr_};
        auto&     channel_msg  = route.set_channel_msg();
        channel_msg.tag        = tag;
        channel_msg.service_id = AnyChannel::getServiceId<Message>(service_name);

        tryPerformOnSerialized(route, [&](const auto prefix) {
            //
            return tryPerformOnSerialized(message, [&](const auto suffix) {
                //
                std::vector<std::uint8_t> buffer;
                std::copy(prefix.begin(), prefix.end(), std::back_inserter(buffer));
                std::copy(suffix.begin(), suffix.end(), std::back_inserter(buffer));
                action(cetl::span<const std::uint8_t>{buffer.data(), buffer.size()});
                return 0;
            });
        });
    }

    // MARK: Data members:

    // NOLINTBEGIN
    ocvsmd::TrackingMemoryResource mr_;
    // NOLINTEND
};

// MARK: - Tests:

TEST_F(TestServerRouter, make)
{
    StrictMock<pipe::ServerPipeMock> server_pipe_mock;
    EXPECT_CALL(server_pipe_mock, deinit()).Times(1);

    const auto server_router = ServerRouter::make(  //
        mr_,
        std::make_unique<pipe::ServerPipeMock::RefWrapper>(server_pipe_mock));
    ASSERT_THAT(server_router, NotNull());
    EXPECT_THAT(server_pipe_mock.event_handler_, IsFalse());
}

TEST_F(TestServerRouter, start)
{
    StrictMock<pipe::ServerPipeMock> server_pipe_mock;
    EXPECT_CALL(server_pipe_mock, deinit()).Times(1);

    const auto server_router = ServerRouter::make(  //
        mr_,
        std::make_unique<pipe::ServerPipeMock::RefWrapper>(server_pipe_mock));
    ASSERT_THAT(server_router, NotNull());
    EXPECT_THAT(server_pipe_mock.event_handler_, IsFalse());

    EXPECT_CALL(server_pipe_mock, start(_)).Times(1);
    server_router->start();
    EXPECT_THAT(server_pipe_mock.event_handler_, IsTrue());
}

TEST_F(TestServerRouter, registerChannel)
{
    using Msg = ocvsmd::common::node_command::ExecCmd_1_0;

    StrictMock<pipe::ServerPipeMock> server_pipe_mock;
    EXPECT_CALL(server_pipe_mock, deinit()).Times(1);

    const auto server_router = ServerRouter::make(  //
        mr_,
        std::make_unique<pipe::ServerPipeMock::RefWrapper>(server_pipe_mock));
    ASSERT_THAT(server_router, NotNull());
    EXPECT_THAT(server_pipe_mock.event_handler_, IsFalse());

    EXPECT_CALL(server_pipe_mock, start(_)).Times(1);
    server_router->start();
    EXPECT_THAT(server_pipe_mock.event_handler_, IsTrue());

    server_router->registerChannel<Msg, Msg>("", [](auto&&, const auto&) {});
}

TEST_F(TestServerRouter, channel_send)
{
    using Msg     = ocvsmd::common::node_command::ExecCmd_1_0;
    using Channel = Channel<Msg, Msg>;

    StrictMock<pipe::ServerPipeMock> server_pipe_mock;
    EXPECT_CALL(server_pipe_mock, deinit()).Times(1);

    const auto server_router = ServerRouter::make(  //
        mr_,
        std::make_unique<pipe::ServerPipeMock::RefWrapper>(server_pipe_mock));
    ASSERT_THAT(server_router, NotNull());
    EXPECT_THAT(server_pipe_mock.event_handler_, IsFalse());

    EXPECT_CALL(server_pipe_mock, start(_)).Times(1);
    server_router->start();
    EXPECT_THAT(server_pipe_mock.event_handler_, IsTrue());

    StrictMock<MockFunction<void(const Channel::EventVar&)>> ch1_event_mock;

    cetl::optional<Channel> maybe_channel;
    server_router->registerChannel<Msg, Msg>("", [&](auto&& ch, const auto& input) {
        //
        ch.setEventHandler(ch1_event_mock.AsStdFunction());
        maybe_channel.emplace(std::forward<Channel>(ch));
        ch1_event_mock.Call(input);
    });
    EXPECT_THAT(maybe_channel.has_value(), IsFalse());

    // Emulate that client posted initial `RouteChannelMsg` on 1/42 tag/client pair.
    //
    EXPECT_CALL(ch1_event_mock, Call(VariantWith<Channel::Input>(_))).Times(1);
    withRouteChannelMsg("", 1, Channel::Input{&mr_}, [&](const auto payload) {
        //
        server_pipe_mock.event_handler_(pipe::ServerPipe::Event::Message{42, payload});
    });

    // Emulate that client posted one more `RouteChannelMsg` on the same 1/42 tag/client pair.
    //
    EXPECT_CALL(ch1_event_mock, Call(VariantWith<Channel::Input>(_))).Times(1);
    withRouteChannelMsg("", 2, Channel::Input{&mr_}, [&](const auto payload) {
        //
        server_pipe_mock.event_handler_(pipe::ServerPipe::Event::Message{42, payload});
    });
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
