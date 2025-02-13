//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "ipc/server_router.hpp"

#include "ipc/channel.hpp"
#include "ipc/ipc_types.hpp"
#include "ipc/pipe/server_pipe.hpp"
#include "ipc_gtest_helpers.hpp"
#include "pipe/server_pipe_mock.hpp"
#include "tracking_memory_resource.hpp"

#include "ocvsmd/common/ipc/Route_0_1.hpp"
#include "ocvsmd/common/svc/node/ExecCmdSvcRequest_0_1.hpp"

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
using testing::Return;
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

    void emulateRouteConnect(const pipe::ServerPipe::ClientId client_id,
                             pipe::ServerPipeMock&            server_pipe_mock,
                             const std::uint8_t               ver_major  = VERSION_MAJOR,  // NOLINT
                             const std::uint8_t               ver_minor  = VERSION_MINOR,
                             const ErrorCode                  error_code = ErrorCode::Success)
    {
        using ocvsmd::common::tryPerformOnSerialized;

        server_pipe_mock.event_handler_(pipe::ServerPipe::Event::Connected{client_id});

        Route_0_1 route{&mr_};
        auto&     rt_conn     = route.set_connect();
        rt_conn.version.major = ver_major;
        rt_conn.version.minor = ver_minor;
        rt_conn.error_code    = static_cast<std::int32_t>(error_code);
        //
        EXPECT_CALL(server_pipe_mock, send(client_id, PayloadOfRouteConnect(mr_)))  //
            .WillOnce(Return(0));
        const int result = tryPerformOnSerialized(route, [&](const auto payload) {
            //
            return server_pipe_mock.event_handler_(pipe::ServerPipe::Event::Message{client_id, payload});
        });
        EXPECT_THAT(result, 0);
    }

    template <typename Msg>
    void emulateRouteChannelMsg(const pipe::ServerPipe::ClientId client_id,
                                pipe::ServerPipeMock&            server_pipe_mock,
                                const std::uint64_t              tag,
                                const Msg&                       msg,
                                std::uint64_t&                   seq,
                                const cetl::string_view          service_name = "")
    {
        using ocvsmd::common::tryPerformOnSerialized;

        Route_0_1 route{&mr_};
        auto&     channel_msg  = route.set_channel_msg();
        channel_msg.tag        = tag;
        channel_msg.sequence   = seq++;
        channel_msg.service_id = AnyChannel::getServiceDesc<Msg>(service_name).id;

        const int result = tryPerformOnSerialized(route, [&](const auto prefix) {
            //
            return tryPerformOnSerialized(msg, [&](const auto suffix) {
                //
                std::vector<std::uint8_t> buffer;
                std::copy(prefix.begin(), prefix.end(), std::back_inserter(buffer));
                std::copy(suffix.begin(), suffix.end(), std::back_inserter(buffer));
                const Payload payload{buffer.data(), buffer.size()};
                return server_pipe_mock.event_handler_(pipe::ServerPipe::Event::Message{client_id, payload});
            });
        });
        EXPECT_THAT(result, 0);
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
        std::make_unique<pipe::ServerPipeMock::Wrapper>(server_pipe_mock));
    ASSERT_THAT(server_router, NotNull());
    EXPECT_THAT(server_pipe_mock.event_handler_, IsFalse());
}

TEST_F(TestServerRouter, start)
{
    StrictMock<pipe::ServerPipeMock> server_pipe_mock;
    EXPECT_CALL(server_pipe_mock, deinit()).Times(1);

    const auto server_router = ServerRouter::make(  //
        mr_,
        std::make_unique<pipe::ServerPipeMock::Wrapper>(server_pipe_mock));
    ASSERT_THAT(server_router, NotNull());
    EXPECT_THAT(server_pipe_mock.event_handler_, IsFalse());

    EXPECT_CALL(server_pipe_mock, start(_)).Times(1);
    EXPECT_THAT(server_router->start(), 0);
    EXPECT_THAT(server_pipe_mock.event_handler_, IsTrue());
}

TEST_F(TestServerRouter, registerChannel)
{
    using Msg     = ocvsmd::common::svc::node::ExecCmdSvcRequest_0_1;
    using Channel = Channel<Msg, Msg>;

    StrictMock<pipe::ServerPipeMock> server_pipe_mock;
    EXPECT_CALL(server_pipe_mock, deinit()).Times(1);

    const auto server_router = ServerRouter::make(  //
        mr_,
        std::make_unique<pipe::ServerPipeMock::Wrapper>(server_pipe_mock));
    ASSERT_THAT(server_router, NotNull());
    EXPECT_THAT(server_pipe_mock.event_handler_, IsFalse());

    EXPECT_CALL(server_pipe_mock, start(_)).Times(1);
    EXPECT_THAT(server_router->start(), 0);
    EXPECT_THAT(server_pipe_mock.event_handler_, IsTrue());

    server_router->registerChannel<Channel>("", [](auto&&, const auto&) {});
}

TEST_F(TestServerRouter, channel_send)
{
    using Msg     = ocvsmd::common::svc::node::ExecCmdSvcRequest_0_1;
    using Channel = Channel<Msg, Msg>;

    StrictMock<pipe::ServerPipeMock> server_pipe_mock;
    EXPECT_CALL(server_pipe_mock, deinit()).Times(1);

    const auto server_router = ServerRouter::make(  //
        mr_,
        std::make_unique<pipe::ServerPipeMock::Wrapper>(server_pipe_mock));
    ASSERT_THAT(server_router, NotNull());
    EXPECT_THAT(server_pipe_mock.event_handler_, IsFalse());

    EXPECT_CALL(server_pipe_mock, start(_)).Times(1);
    EXPECT_THAT(server_router->start(), 0);
    EXPECT_THAT(server_pipe_mock.event_handler_, IsTrue());

    StrictMock<MockFunction<void(const Channel::EventVar&)>> ch_event_mock;

    cetl::optional<Channel> maybe_channel;
    server_router->registerChannel<Channel>("", [&](Channel&& ch, const auto& input) {
        //
        ch.subscribe(ch_event_mock.AsStdFunction());
        maybe_channel = std::move(ch);
        ch_event_mock.Call(input);
    });
    EXPECT_THAT(maybe_channel.has_value(), IsFalse());

    // Emulate that client #42 is connected.
    //
    constexpr std::uint64_t cl_id = 42;
    EXPECT_CALL(ch_event_mock, Call(VariantWith<Channel::Connected>(_))).Times(1);
    emulateRouteConnect(cl_id, server_pipe_mock);

    // Emulate that client posted initial `RouteChannelMsg` on 42/7 client/tag pair.
    //
    const std::uint64_t tag = 7;
    std::uint64_t       seq = 0;
    EXPECT_CALL(ch_event_mock, Call(VariantWith<Channel::Input>(_))).Times(1);
    emulateRouteChannelMsg(cl_id, server_pipe_mock, tag, Channel::Input{&mr_}, seq);
    ASSERT_THAT(maybe_channel.has_value(), IsTrue());
    EXPECT_CALL(server_pipe_mock, send(cl_id, PayloadOfRouteChannelEnd(mr_, tag, ErrorCode::Success)))  //
        .WillOnce(Return(0));

    // Emulate that client posted one more `RouteChannelMsg` on the same 42/7 client/tag pair.
    //
    EXPECT_CALL(ch_event_mock, Call(VariantWith<Channel::Input>(_))).Times(1);
    emulateRouteChannelMsg(cl_id, server_pipe_mock, tag, Channel::Input{&mr_}, seq);

    seq = 0;
    const Channel::Output msg{&mr_};
    EXPECT_CALL(server_pipe_mock, send(cl_id, PayloadOfRouteChannelMsg(msg, mr_, tag, seq++)))  //
        .WillOnce(Return(0));
    EXPECT_THAT(maybe_channel->send(msg), 0);  // NOLINT

    EXPECT_CALL(server_pipe_mock, send(cl_id, PayloadOfRouteChannelMsg(msg, mr_, tag, seq++)))  //
        .WillOnce(Return(0));
    EXPECT_THAT(maybe_channel->send(msg), 0);  // NOLINT
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace
