//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "server_router.hpp"

#include "dsdl_helpers.hpp"
#include "gateway.hpp"
#include "ipc_types.hpp"
#include "pipe/server_pipe.hpp"

#include "ocvsmd/common/ipc/RouteChannelMsg_1_0.hpp"
#include "ocvsmd/common/ipc/RouteConnect_1_0.hpp"
#include "ocvsmd/common/ipc/Route_1_0.hpp"
#include "uavcan/primitive/Empty_1_0.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/visit_helpers.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sys/syslog.h>
#include <unordered_map>
#include <unordered_set>
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
    ServerRouterImpl(cetl::pmr::memory_resource& memory, pipe::ServerPipe::Ptr server_pipe)
        : memory_{memory}
        , server_pipe_{std::move(server_pipe)}
    {
        CETL_DEBUG_ASSERT(server_pipe_, "");
    }

    // ServerRouter

    CETL_NODISCARD cetl::pmr::memory_resource& memory() override
    {
        return memory_;
    }

    CETL_NODISCARD int start() override
    {
        return server_pipe_->start([this](const auto& pipe_event_var) {
            //
            return cetl::visit(
                [this](const auto& pipe_event) {
                    //
                    return handlePipeEvent(pipe_event);
                },
                pipe_event_var);
        });
    }

    void registerChannelFactory(const detail::ServiceId  service_id,  //
                                TypeErasedChannelFactory channel_factory) override
    {
        service_id_to_channel_factory_[service_id] = std::move(channel_factory);
    }

private:
    struct Endpoint final
    {
        using Tag      = std::uint64_t;
        using ClientId = pipe::ServerPipe::ClientId;

        Endpoint(const Tag tag, ClientId client_id) noexcept
            : tag_{tag}
            , client_id_{client_id}
        {
        }

        CETL_NODISCARD Tag getTag() const noexcept
        {
            return tag_;
        }

        CETL_NODISCARD ClientId getClientId() const noexcept
        {
            return client_id_;
        }

        // Hasher

        CETL_NODISCARD bool operator==(const Endpoint& other) const noexcept
        {
            return tag_ == other.tag_ && client_id_ == other.client_id_;
        }

        struct Hasher final
        {
            CETL_NODISCARD std::size_t operator()(const Endpoint& endpoint) const noexcept
            {
                const std::size_t h1 = std::hash<Tag>{}(endpoint.tag_);
                const std::size_t h2 = std::hash<ClientId>{}(endpoint.client_id_);
                return h1 ^ (h2 << 1ULL);
            }

        };  // Hasher

    private:
        const Tag      tag_;
        const ClientId client_id_;

    };  // Endpoint

    class GatewayImpl final : public std::enable_shared_from_this<GatewayImpl>, public detail::Gateway
    {
        struct Private
        {
            explicit Private() = default;
        };

    public:
        CETL_NODISCARD static std::shared_ptr<GatewayImpl> create(ServerRouterImpl& router, const Endpoint& endpoint)
        {
            return std::make_shared<GatewayImpl>(Private(), router, endpoint);
        }

        GatewayImpl(Private, ServerRouterImpl& router, const Endpoint& endpoint)
            : router_{router}
            , endpoint_{endpoint}
            , next_sequence_{0}
        {
            ::syslog(LOG_DEBUG, "Gateway(cl=%zu, tag=%zu).", endpoint.getClientId(), endpoint.getTag());
        }

        GatewayImpl(const GatewayImpl&)                = delete;
        GatewayImpl(GatewayImpl&&) noexcept            = delete;
        GatewayImpl& operator=(const GatewayImpl&)     = delete;
        GatewayImpl& operator=(GatewayImpl&&) noexcept = delete;

        ~GatewayImpl()
        {
            router_.unregisterGateway(endpoint_, true);
            ::syslog(LOG_DEBUG, "~Gateway(cl=%zu, tag=%zu).", endpoint_.getClientId(), endpoint_.getTag());
        }

        // detail::Gateway

        CETL_NODISCARD int send(const detail::ServiceId service_id, const Payload payload) override
        {
            if (!router_.isConnected(endpoint_))
            {
                return static_cast<int>(ErrorCode::NotConnected);
            }

            Route_1_0 route{&router_.memory_};

            auto& channel_msg      = route.set_channel_msg();
            channel_msg.tag        = endpoint_.getTag();
            channel_msg.sequence   = next_sequence_++;
            channel_msg.service_id = service_id;

            return tryPerformOnSerialized(route, [this, payload](const auto prefix) {
                //
                return router_.server_pipe_->send(endpoint_.getClientId(), {{prefix, payload}});
            });
        }

        void event(const Event::Var& event) override
        {
            if (event_handler_)
            {
                event_handler_(event);
            }
        }

        void subscribe(EventHandler event_handler) override
        {
            if (event_handler)
            {
                event_handler_ = std::move(event_handler);
                router_.registerGateway(endpoint_, *this);
            }
            else
            {
                event_handler_ = nullptr;
                router_.unregisterGateway(endpoint_, false);
            }
        }

    private:
        ServerRouterImpl& router_;
        const Endpoint    endpoint_;
        std::uint64_t     next_sequence_;
        EventHandler      event_handler_;

    };  // GatewayImpl

    using ServiceIdToChannelFactory = std::unordered_map<detail::ServiceId, TypeErasedChannelFactory>;
    using EndpointToWeakGateway     = std::unordered_map<Endpoint, detail::Gateway::WeakPtr, Endpoint::Hasher>;

    CETL_NODISCARD bool isConnected(const Endpoint& endpoint) const noexcept
    {
        return connected_client_ids_.find(endpoint.getClientId()) != connected_client_ids_.end();
    }

    void registerGateway(const Endpoint& endpoint, GatewayImpl& gateway)
    {
        endpoint_to_gateway_[endpoint] = gateway.shared_from_this();
        if (isConnected(endpoint))
        {
            gateway.event(detail::Gateway::Event::Connected{});
        }
    }

    void unregisterGateway(const Endpoint& endpoint, const bool is_disposed = false)
    {
        endpoint_to_gateway_.erase(endpoint);

        // Notify "remote" router about the gateway disposal.
        // The router will deliver "disconnected" event to the counterpart gateway (if it exists).
        //
        if (is_disposed && isConnected(endpoint))
        {
        }
    }

    CETL_NODISCARD int handlePipeEvent(const pipe::ServerPipe::Event::Connected conn)
    {
        connected_client_ids_.insert(conn.client_id);
        return 0;
    }

    CETL_NODISCARD int handlePipeEvent(const pipe::ServerPipe::Event::Message& msg)
    {
        Route_1_0  route_msg{&memory_};
        const auto result_size = tryDeserializePayload(msg.payload, route_msg);
        if (!result_size.has_value())
        {
            return EINVAL;
        }

        // Cut routing stuff from the payload - remaining is the actual message payload.
        const auto msg_payload = msg.payload.subspan(result_size.value());

        cetl::visit(cetl::make_overloaded(
                        //
                        [this](const uavcan::primitive::Empty_1_0&) {},
                        [this, &msg](const RouteConnect_1_0& route_conn) {
                            //
                            handleRouteConnect(msg.client_id, route_conn);
                        },
                        [this, &msg, msg_payload](const RouteChannelMsg_1_0& route_ch_msg) {
                            //
                            handleRouteChannelMsg(msg.client_id, route_ch_msg, msg_payload);
                        },
                        [this, &msg, msg_payload](const RouteChannelEnd_1_0& route_ch_end) {
                            //
                            handleRouteChannelEnd(msg.client_id, route_ch_end);
                        }),
                    route_msg.union_value);

        return 0;

        return 0;
    }

    CETL_NODISCARD static int handlePipeEvent(const pipe::ServerPipe::Event::Disconnected)
    {
        // TODO: Implement! disconnected for all gateways which belong to the corresponding client id
        return 0;
    }

    void handleRouteConnect(const pipe::ServerPipe::ClientId client_id, const RouteConnect_1_0&) const
    {
        // TODO: log client route connection

        Route_1_0 route{&memory_};

        auto& route_conn         = route.set_connect();
        route_conn.version.major = VERSION_MAJOR;
        route_conn.version.minor = VERSION_MINOR;

        const int result = tryPerformOnSerialized(route, [this, client_id](const auto payload) {
            //
            return server_pipe_->send(client_id, {{payload}});
        });
        (void) result;
    }

    void handleRouteChannelMsg(const pipe::ServerPipe::ClientId client_id,
                               const RouteChannelMsg_1_0&       route_ch_msg,
                               const Payload                    msg_payload)
    {
        const Endpoint endpoint{route_ch_msg.tag, client_id};

        const auto ep_to_gw = endpoint_to_gateway_.find(endpoint);
        if (ep_to_gw != endpoint_to_gateway_.end())
        {
            auto gateway = ep_to_gw->second.lock();
            if (gateway)
            {
                gateway->event(detail::Gateway::Event::Message{route_ch_msg.sequence, msg_payload});
                return;
            }
        }

        // Only the very first message in the sequence is considered to trigger channel factory.
        if (route_ch_msg.sequence == 0)
        {
            const auto si_to_cf = service_id_to_channel_factory_.find(route_ch_msg.service_id);
            if (si_to_cf != service_id_to_channel_factory_.end())
            {
                auto gateway = GatewayImpl::create(*this, endpoint);
                si_to_cf->second(gateway, msg_payload);
            }
        }

        // TODO: log unsolicited message
    }

    void handleRouteChannelEnd(const pipe::ServerPipe::ClientId client_id, const RouteChannelEnd_1_0& route_ch_end) {}

    cetl::pmr::memory_resource&            memory_;
    pipe::ServerPipe::Ptr                  server_pipe_;
    EndpointToWeakGateway                  endpoint_to_gateway_;
    ServiceIdToChannelFactory              service_id_to_channel_factory_;
    std::unordered_set<Endpoint::ClientId> connected_client_ids_;

};  // ClientRouterImpl

}  // namespace

CETL_NODISCARD ServerRouter::Ptr ServerRouter::make(cetl::pmr::memory_resource& memory,
                                                    pipe::ServerPipe::Ptr       server_pipe)
{
    return std::make_unique<ServerRouterImpl>(memory, std::move(server_pipe));
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
