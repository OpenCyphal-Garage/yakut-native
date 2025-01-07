//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "server_router.hpp"

#include "dsdl_helpers.hpp"
#include "gateway.hpp"
#include "pipe/pipe_types.hpp"
#include "pipe/server_pipe.hpp"

#include "ocvsmd/common/ipc/RouteChannelMsg_1_0.hpp"
#include "ocvsmd/common/ipc/RouteConnect_1_0.hpp"
#include "ocvsmd/common/ipc/Route_1_0.hpp"
#include "uavcan/primitive/Empty_1_0.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/visit_helpers.hpp>

#include <array>
#include <cerrno>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace
{

class ServerRouterImpl final : public ServerRouter
{
public:
    explicit ServerRouterImpl(cetl::pmr::memory_resource& memory, pipe::ServerPipe::Ptr server_pipe)
        : memory_{memory}
        , server_pipe_{std::move(server_pipe)}
    {
        CETL_DEBUG_ASSERT(server_pipe_, "");
    }

    // ServerRouter

    cetl::pmr::memory_resource& memory() override
    {
        return memory_;
    }

    void start() override
    {
        server_pipe_->start([this](const auto& pipe_event_var) {
            //
            return cetl::visit(
                [this](const auto& pipe_event) {
                    //
                    return handlePipeEvent(pipe_event);
                },
                pipe_event_var);
        });
    }

    void registerChannelFactory(  //
        const detail::MsgTypeId  input_type_id,
        TypeErasedChannelFactory channel_factory) override
    {
        type_id_to_channel_factory_[input_type_id] = std::move(channel_factory);
    }

private:
    using Tag      = std::uint64_t;
    using ClientId = pipe::ServerPipe::ClientId;

    class GatewayImpl final : public std::enable_shared_from_this<GatewayImpl>, public detail::Gateway
    {
        struct Private
        {
            explicit Private() = default;
        };

    public:
        static std::shared_ptr<GatewayImpl> create(const Tag tag, ServerRouterImpl& router, const ClientId client_id)
        {
            return std::make_shared<GatewayImpl>(Private(), tag, router, client_id);
        }

        GatewayImpl(Private, const Tag tag, ServerRouterImpl& router, const ClientId client_id)
            : tag_{tag}
            , router_{router}
            , client_id_{client_id}
        {
        }

        GatewayImpl(const GatewayImpl&)                = delete;
        GatewayImpl(GatewayImpl&&) noexcept            = delete;
        GatewayImpl& operator=(const GatewayImpl&)     = delete;
        GatewayImpl& operator=(GatewayImpl&&) noexcept = delete;

        ~GatewayImpl()
        {
            setEventHandler(nullptr);
        }

        void send(const detail::MsgTypeId type_id, const pipe::Payload payload) override
        {
            Route_1_0 route{&router_.memory_};
            auto&     channel_msg = route.set_channel_msg();
            channel_msg.tag       = tag_;
            channel_msg.type_id   = type_id;

            tryPerformOnSerialized(route, [this, payload](const auto prefix) {
                //
                std::array<pipe::Payload, 2> fragments{prefix, payload};
                return router_.server_pipe_->sendMessage(client_id_, fragments);
            });
        }

        void event(const Event::Var& event) override
        {
            if (event_handler_)
            {
                event_handler_(event);
            }
        }

        void setEventHandler(EventHandler event_handler) override
        {
            if (event_handler)
            {
                event_handler_                = std::move(event_handler);
                router_.tag_to_gateway_[tag_] = shared_from_this();
            }
            else
            {
                router_.tag_to_gateway_.erase(tag_);
            }
        }

    private:
        const Tag         tag_;
        ServerRouterImpl& router_;
        const ClientId    client_id_;
        EventHandler      event_handler_;

    };  // GatewayImpl

    static int handlePipeEvent(const pipe::ServerPipe::Event::Connected)
    {
        // TODO: Implement!
        return 0;
    }

    int handlePipeEvent(const pipe::ServerPipe::Event::Message& msg)
    {
        Route_1_0  route_msg{&memory_};
        const auto result_size = tryDeserializePayload(msg.payload, route_msg);
        if (!result_size.has_value())
        {
            return EINVAL;
        }

        const auto remaining_payload = msg.payload.subspan(result_size.value());

        cetl::visit(cetl::make_overloaded(
                        //
                        [this](const uavcan::primitive::Empty_1_0&) {},
                        [this, &msg](const RouteConnect_1_0& route_conn) {
                            //
                            handleRouteConnect(msg.client_id, route_conn);
                        },
                        [this, &msg, remaining_payload](const RouteChannelMsg_1_0& route_channel) {
                            //
                            handleRouteChannelMsg(msg.client_id, route_channel, remaining_payload);
                        }),
                    route_msg.union_value);

        return 0;

        return 0;
    }

    static int handlePipeEvent(const pipe::ServerPipe::Event::Disconnected)
    {
        // TODO: Implement! disconnected for all gateways which belong to the corresponding client id
        return 0;
    }

    void handleRouteConnect(const ClientId client_id, const RouteConnect_1_0&) const
    {
        // TODO: log client route connection

        Route_1_0 route{&memory_};
        auto&     route_conn     = route.set_connect();
        route_conn.version.major = VERSION_MAJOR;
        route_conn.version.minor = VERSION_MINOR;

        tryPerformOnSerialized<Route_1_0>(route, [this, client_id](const auto payload) {
            //
            std::array<pipe::Payload, 1> payloads{payload};
            return server_pipe_->sendMessage(client_id, payloads);
        });
    }

    void handleRouteChannelMsg(const ClientId             client_id,
                               const RouteChannelMsg_1_0& route_channel_msg,
                               pipe::Payload              payload)
    {
        const auto tag_it = tag_to_gateway_.find(route_channel_msg.tag);
        if (tag_it != tag_to_gateway_.end())
        {
            auto gateway = tag_it->second.lock();
            if (gateway)
            {
                gateway->event(detail::Gateway::Event::Message{payload});
            }
            return;
        }

        const auto ch_factory_it = type_id_to_channel_factory_.find(route_channel_msg.type_id);
        if (ch_factory_it != type_id_to_channel_factory_.end())
        {
            auto gateway = GatewayImpl::create(route_channel_msg.tag, *this, client_id);

            tag_to_gateway_[route_channel_msg.tag] = gateway;
            ch_factory_it->second(gateway, payload);
        }

        // TODO: log unsolicited message
    }

    cetl::pmr::memory_resource&                                     memory_;
    pipe::ServerPipe::Ptr                                           server_pipe_;
    std::unordered_map<detail::MsgTypeId, TypeErasedChannelFactory> type_id_to_channel_factory_;
    std::unordered_map<Tag, detail::Gateway::WeakPtr>               tag_to_gateway_;

};  // ClientRouterImpl

}  // namespace

ServerRouter::Ptr ServerRouter::make(cetl::pmr::memory_resource& memory, pipe::ServerPipe::Ptr server_pipe)
{
    return std::make_unique<ServerRouterImpl>(memory, std::move(server_pipe));
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
