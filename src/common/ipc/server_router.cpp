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
#include "ocvsmd/common/ipc/RouteConnect_0_1.hpp"
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
            ::syslog(LOG_DEBUG, "Gateway(cl=%zu, tag=%zu).", endpoint.getClientId(), endpoint.getTag());  // NOLINT
        }

        GatewayImpl(const GatewayImpl&)                = delete;
        GatewayImpl(GatewayImpl&&) noexcept            = delete;
        GatewayImpl& operator=(const GatewayImpl&)     = delete;
        GatewayImpl& operator=(GatewayImpl&&) noexcept = delete;

        ~GatewayImpl()
        {
            ::syslog(LOG_DEBUG, "~Gateway(cl=%zu, tag=%zu).", endpoint_.getClientId(), endpoint_.getTag());  // NOLINT

            performWithoutThrowing([this] {
                //
                router_.onGatewayDisposal(endpoint_);
            });
        }

        // detail::Gateway

        CETL_NODISCARD int send(const detail::ServiceId service_id, const Payload payload) override
        {
            if (!router_.isConnected(endpoint_))
            {
                return static_cast<int>(ErrorCode::NotConnected);
            }
            if (!router_.isRegisteredGateway(endpoint_))
            {
                return static_cast<int>(ErrorCode::Disconnected);
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
            event_handler_ = std::move(event_handler);
            router_.onGatewaySubscription(endpoint_);
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

    CETL_NODISCARD bool isRegisteredGateway(const Endpoint& endpoint) const noexcept
    {
        return endpoint_to_gateway_.find(endpoint) != endpoint_to_gateway_.end();
    }

    template <typename Action>
    void findAndActOnRegisteredGateway(const Endpoint endpoint, Action&& action)
    {
        const auto ep_to_gw = endpoint_to_gateway_.find(endpoint);
        if (ep_to_gw != endpoint_to_gateway_.end())
        {
            const auto gateway = ep_to_gw->second.lock();
            if (gateway)
            {
                std::forward<Action>(action)(*gateway, ep_to_gw);
            }
        }
    }

    void onGatewaySubscription(const Endpoint endpoint)
    {
        if (isConnected(endpoint))
        {
            findAndActOnRegisteredGateway(endpoint, [](auto& gateway, auto) {
                //
                gateway.event(detail::Gateway::Event::Connected{});
            });
        }
    }

    /// Unregisters the gateway associated with the given endpoint.
    ///
    /// Called on the gateway disposal (correspondingly on its channel destruction).
    /// The "dying" gateway wishes to notify the remote client router about its disposal.
    /// This local router fulfills the wish if the gateway was registered and the client router is connected.
    ///
    void onGatewayDisposal(const Endpoint& endpoint)
    {
        const bool was_registered = (endpoint_to_gateway_.erase(endpoint) > 0);

        // Notify remote client router about the gateway disposal (aka channel completion).
        // The router will propagate "ChEnd" event to the counterpart gateway (if it's registered).
        //
        if (was_registered && isConnected(endpoint))
        {
            Route_1_0 route{&memory_};
            auto&     channel_end  = route.set_channel_end();
            channel_end.tag        = endpoint.getTag();
            channel_end.error_code = 0;  // No error b/c it's a normal channel completion.

            const int error = tryPerformOnSerialized(route, [this, &endpoint](const auto payload) {
                //
                return server_pipe_->send(endpoint.getClientId(), {{payload}});
            });
            // Best efforts strategy - gateway anyway is gone, so nowhere to report.
            (void) error;
        }
    }

    CETL_NODISCARD static int handlePipeEvent(const pipe::ServerPipe::Event::Connected& pipe_conn)
    {
        ::syslog(LOG_DEBUG, "Pipe is connected (cl=%zu).", pipe_conn.client_id);  // NOLINT

        // It's not enough to consider the client router connected by the pipe event.
        // We gonna wait for `RouteConnect` negotiation (see `handleRouteConnect`).
        // But for now everything is fine.
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
                        [this, &msg](const RouteConnect_0_1& route_conn) {
                            //
                            handleRouteConnect(msg.client_id, route_conn);
                        },
                        [this, &msg, msg_payload](const RouteChannelMsg_1_0& route_ch_msg) {
                            //
                            handleRouteChannelMsg(msg.client_id, route_ch_msg, msg_payload);
                        },
                        [this, &msg](const RouteChannelEnd_1_0& route_ch_end) {
                            //
                            handleRouteChannelEnd(msg.client_id, route_ch_end);
                        }),
                    route_msg.union_value);

        return 0;
    }

    CETL_NODISCARD static int handlePipeEvent(const pipe::ServerPipe::Event::Disconnected& disconn)
    {
        ::syslog(LOG_DEBUG, "Pipe is disconnected (cl=%zu).", disconn.client_id);  // NOLINT

        // TODO: Implement! disconnected for all gateways which belong to the corresponding client id
        return 0;
    }

    void handleRouteConnect(const pipe::ServerPipe::ClientId client_id, const RouteConnect_0_1& rt_conn)
    {
        ::syslog(LOG_DEBUG,  // NOLINT
                 "Route connect request (cl=%zu, ver='%d.%d', err=%d).",
                 client_id,
                 static_cast<int>(rt_conn.version.major),
                 static_cast<int>(rt_conn.version.minor),
                 static_cast<int>(rt_conn.error_code));

        Route_1_0 route{&memory_};
        auto&     route_conn     = route.set_connect();
        route_conn.version.major = VERSION_MAJOR;
        route_conn.version.minor = VERSION_MINOR;
        // In the future, we might have version comparison logic here,
        // and potentially refuse the connection if the versions are incompatible.
        route_conn.error_code = 0;
        //
        const int error = tryPerformOnSerialized(route, [this, client_id](const auto payload) {
            //
            return server_pipe_->send(client_id, {{payload}});
        });
        if (0 == error)
        {
            connected_client_ids_.insert(client_id);
        }
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
                auto gateway                   = GatewayImpl::create(*this, endpoint);
                endpoint_to_gateway_[endpoint] = gateway;
                si_to_cf->second(gateway, msg_payload);
            }
        }

        // TODO: log unsolicited message
    }

    void handleRouteChannelEnd(const pipe::ServerPipe::ClientId client_id, const RouteChannelEnd_1_0& route_ch_end)
    {
        ::syslog(LOG_DEBUG, "Route Ch End (tag=%zu, err=%d).", route_ch_end.tag, route_ch_end.error_code);  // NOLINT

        const Endpoint endpoint{route_ch_end.tag, client_id};
        const auto     error_code = static_cast<ErrorCode>(route_ch_end.error_code);

        findAndActOnRegisteredGateway(endpoint, [this, error_code](auto& gateway, auto found_it) {
            //
            endpoint_to_gateway_.erase(found_it);
            gateway.event(detail::Gateway::Event::Completed{error_code});
        });
    }

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
