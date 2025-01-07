//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "ipc/client_router.hpp"

#include "cetl_gtest_helpers.hpp"  // NOLINT(misc-include-cleaner)
#include "ipc/channel.hpp"
#include "ipc/pipe/client_pipe.hpp"
#include "pipe/client_pipe_mock.hpp"
#include "tracking_memory_resource.hpp"

#include "ocvsmd/common/ipc/RouteConnect_1_0.hpp"
#include "ocvsmd/common/ipc/Route_1_0.hpp"
#include "ocvsmd/common/node_command/ExecCmd_1_0.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

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
using testing::Return;
using testing::SizeIs;
using testing::IsEmpty;
using testing::IsFalse;
using testing::NotNull;
using testing::StrictMock;
using testing::ElementsAre;
using testing::VariantWith;
using testing::MockFunction;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

class TestClientRouter : public testing::Test
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

    template <typename Action>
    void withRouteConnect(const RouteConnect_1_0& connect, Action action)
    {
        using ocvsmd::common::tryPerformOnSerialized;

        Route_1_0 route{&mr_};
        route.set_connect(connect);

        tryPerformOnSerialized(route, [&](const auto payload) {
            //
            action(payload);
            return 0;
        });
    }

    template <typename Message, typename Action>
    void withRouteChannelMsg(const std::uint64_t tag, const Message& message, Action action)
    {
        using ocvsmd::common::tryPerformOnSerialized;

        Route_1_0 route{&mr_};
        auto&     channel_msg = route.set_channel_msg();
        channel_msg.tag       = tag;
        channel_msg.type_id   = AnyChannel::getTypeId<Message>();

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

TEST_F(TestClientRouter, make)
{
    StrictMock<pipe::ClientPipeMock> client_pipe_mock;

    const auto client_router = ClientRouter::make(  //
        mr_,
        std::make_unique<pipe::ClientPipeMock::RefWrapper>(client_pipe_mock));
    ASSERT_THAT(client_router, NotNull());
    EXPECT_THAT(client_pipe_mock.event_handler_, IsFalse());

    EXPECT_CALL(client_pipe_mock, deinit()).Times(1);
}

TEST_F(TestClientRouter, start)
{
    StrictMock<pipe::ClientPipeMock> client_pipe_mock;

    const auto client_router = ClientRouter::make(  //
        mr_,
        std::make_unique<pipe::ClientPipeMock::RefWrapper>(client_pipe_mock));
    ASSERT_THAT(client_router, NotNull());
    EXPECT_THAT(client_pipe_mock.event_handler_, IsFalse());

    EXPECT_CALL(client_pipe_mock, start(_)).Times(1);
    client_router->start();
    EXPECT_THAT(client_pipe_mock.event_handler_, IsTrue());

    EXPECT_CALL(client_pipe_mock, deinit()).Times(1);
}

TEST_F(TestClientRouter, makeChannel)
{
    using Msg = ocvsmd::common::node_command::ExecCmd_1_0;

    StrictMock<pipe::ClientPipeMock> client_pipe_mock;
    EXPECT_CALL(client_pipe_mock, deinit()).Times(1);

    const auto client_router = ClientRouter::make(  //
        mr_,
        std::make_unique<pipe::ClientPipeMock::RefWrapper>(client_pipe_mock));
    ASSERT_THAT(client_router, NotNull());

    EXPECT_CALL(client_pipe_mock, start(_)).Times(1);
    client_router->start();

    const auto channel = client_router->makeChannel<Msg, Msg>();
    (void) channel;
}

TEST_F(TestClientRouter, makeChannel_send)
{
    using Msg = ocvsmd::common::node_command::ExecCmd_1_0;

    StrictMock<pipe::ClientPipeMock> client_pipe_mock;
    EXPECT_CALL(client_pipe_mock, deinit()).Times(1);

    const auto client_router = ClientRouter::make(  //
        mr_,
        std::make_unique<pipe::ClientPipeMock::RefWrapper>(client_pipe_mock));
    ASSERT_THAT(client_router, NotNull());

    EXPECT_CALL(client_pipe_mock, start(_)).Times(1);
    client_router->start();

    auto channel = client_router->makeChannel<Msg, Msg>();

    Msg msg{&mr_};

    EXPECT_CALL(client_pipe_mock, sendMessage(SizeIs(2))).WillOnce(Return(0));
    channel.send(msg);

    msg.some_stuff.push_back(-1);
    msg.some_stuff.push_back('X');
    EXPECT_CALL(client_pipe_mock, sendMessage(SizeIs(2))).WillOnce(Return(0));
    channel.send(msg);
}

TEST_F(TestClientRouter, makeChannel_receive_events)
{
    using Msg = ocvsmd::common::node_command::ExecCmd_1_0;
    ;
    using Channel = Channel<Msg, Msg>;

    StrictMock<pipe::ClientPipeMock> client_pipe_mock;
    EXPECT_CALL(client_pipe_mock, deinit()).Times(1);

    const auto client_router = ClientRouter::make(  //
        mr_,
        std::make_unique<pipe::ClientPipeMock::RefWrapper>(client_pipe_mock));
    ASSERT_THAT(client_router, NotNull());

    EXPECT_CALL(client_pipe_mock, start(_)).Times(1);
    client_router->start();

    StrictMock<MockFunction<void(const Channel::EventVar&)>> ch1_event_mock;
    StrictMock<MockFunction<void(const Channel::EventVar&)>> ch2_event_mock;

    auto channel1 = client_router->makeChannel<Msg, Msg>();
    channel1.setEventHandler(ch1_event_mock.AsStdFunction());

    auto channel2 = client_router->makeChannel<Msg, Msg>();
    channel2.setEventHandler(ch2_event_mock.AsStdFunction());

    // Emulate that we've got pipe connected - `RouteConnect` should be sent to the server.
    //
    EXPECT_CALL(client_pipe_mock, sendMessage(_)).WillOnce(Return(0));  // RouteConnect -> server
    client_pipe_mock.event_handler_(pipe::ClientPipe::Event::Connected{});

    // Emulate that server responded with its `RouteConnect` - all channels should be notified.
    //
    EXPECT_CALL(ch1_event_mock, Call(VariantWith<Channel::Connected>(_))).Times(1);
    EXPECT_CALL(ch2_event_mock, Call(VariantWith<Channel::Connected>(_))).Times(1);
    withRouteConnect(RouteConnect_1_0{{1, 2, &mr_}, &mr_}, [&](const auto payload) {
        //
        client_pipe_mock.event_handler_(pipe::ClientPipe::Event::Message{payload});
    });

    // Emulate that server posted `RouteChannelMsg` on tag #1.
    //
    EXPECT_CALL(ch1_event_mock, Call(VariantWith<Channel::Input>(_))).Times(1);
    withRouteChannelMsg(1, Channel::Input{&mr_}, [&](const auto payload) {
        //
        client_pipe_mock.event_handler_(pipe::ClientPipe::Event::Message{payload});
    });

    // Emulate that server posted `RouteChannelMsg` on tag #2.
    //
    EXPECT_CALL(ch2_event_mock, Call(VariantWith<Channel::Input>(_))).Times(1);
    withRouteChannelMsg(2, Channel::Input{&mr_}, [&](const auto payload) {
        //
        client_pipe_mock.event_handler_(pipe::ClientPipe::Event::Message{payload});
    });

    // Emulate that the pipe is disconnected - all channels should be notified.
    //
    EXPECT_CALL(ch1_event_mock, Call(VariantWith<Channel::Disconnected>(_))).Times(1);
    EXPECT_CALL(ch2_event_mock, Call(VariantWith<Channel::Disconnected>(_))).Times(1);
    client_pipe_mock.event_handler_(pipe::ClientPipe::Event::Disconnected{});
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
