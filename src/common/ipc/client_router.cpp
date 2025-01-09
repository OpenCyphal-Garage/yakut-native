//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "client_router.hpp"

#include "dsdl_helpers.hpp"
#include "gateway.hpp"
#include "ipc_types.hpp"
#include "pipe/client_pipe.hpp"

#include "ocvsmd/common/ipc/RouteChannelEnd_1_0.hpp"
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
#include <utility>
#include <vector>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace
{

class ClientRouterImpl final : public ClientRouter
{
public:
    ClientRouterImpl(cetl::pmr::memory_resource& memory, pipe::ClientPipe::Ptr client_pipe)
        : memory_{memory}
        , client_pipe_{std::move(client_pipe)}
        , next_tag_{0}
        , is_connected_{false}
    {
        CETL_DEBUG_ASSERT(client_pipe_, "");
    }

    // ClientRouter

    CETL_NODISCARD cetl::pmr::memory_resource& memory() override
    {
        return memory_;
    }

    CETL_NODISCARD int start() override
    {
        return client_pipe_->start([this](const auto& pipe_event_var) {
            //
            return cetl::visit(
                [this](const auto& pipe_event) {
                    //
                    return handlePipeEvent(pipe_event);
                },
                pipe_event_var);
        });
    }

    CETL_NODISCARD detail::Gateway::Ptr makeGateway() override
    {
        const Endpoint endpoint{next_tag_++};

        auto gateway                   = GatewayImpl::create(*this, endpoint);
        endpoint_to_gateway_[endpoint] = gateway;
        return gateway;
    }

private:
    struct Endpoint final
    {
        using Tag = std::uint64_t;

        explicit Endpoint(const Tag tag) noexcept
            : tag_{tag}
        {
        }

        CETL_NODISCARD Tag getTag() const noexcept
        {
            return tag_;
        }

        // Hasher

        CETL_NODISCARD bool operator==(const Endpoint& other) const noexcept
        {
            return tag_ == other.tag_;
        }

        struct Hasher final
        {
            CETL_NODISCARD std::size_t operator()(const Endpoint& endpoint) const noexcept
            {
                return std::hash<Tag>{}(endpoint.tag_);
            }

        };  // Hasher

    private:
        const Tag tag_;

    };  // Endpoint

    class GatewayImpl final : public std::enable_shared_from_this<GatewayImpl>, public detail::Gateway
    {
        struct Private
        {
            explicit Private() = default;
        };

    public:
        CETL_NODISCARD static std::shared_ptr<GatewayImpl> create(ClientRouterImpl& router, const Endpoint& endpoint)
        {
            return std::make_shared<GatewayImpl>(Private(), router, endpoint);
        }

        GatewayImpl(Private, ClientRouterImpl& router, const Endpoint& endpoint)
            : router_{router}
            , endpoint_{endpoint}
            , next_sequence_{0}
        {
            ::syslog(LOG_DEBUG, "Gateway(tag=%zu).", endpoint.getTag());
        }

        GatewayImpl(const GatewayImpl&)                = delete;
        GatewayImpl(GatewayImpl&&) noexcept            = delete;
        GatewayImpl& operator=(const GatewayImpl&)     = delete;
        GatewayImpl& operator=(GatewayImpl&&) noexcept = delete;

        ~GatewayImpl()
        {
            ::syslog(LOG_DEBUG, "~Gateway(tag=%zu, seq=%zu).", endpoint_.getTag(), next_sequence_);

            // `next_sequence_ == 0` means that this gateway was never used for sending messages,
            // and so remote router never knew about it (its tag) - no need to post "ChEnd" event.
            router_.onGatewayDisposal(endpoint_, next_sequence_ > 0);
        }

        // detail::Gateway

        CETL_NODISCARD int send(const detail::ServiceId service_id, const Payload payload) override
        {
            if (!router_.is_connected_)
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
                return router_.client_pipe_->send({{prefix, payload}});
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
        ClientRouterImpl& router_;
        const Endpoint    endpoint_;
        std::uint64_t     next_sequence_;
        EventHandler      event_handler_;

    };  // GatewayImpl

    using EndpointToWeakGateway = std::unordered_map<Endpoint, detail::Gateway::WeakPtr, Endpoint::Hasher>;

    CETL_NODISCARD bool isConnected(const Endpoint&) const noexcept
    {
        return is_connected_;
    }

    template <typename Action>
    void findRegisteredGateway(const Endpoint endpoint, Action&& action)
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

    template <typename Action>
    void forEachRegisteredGateway(Action action)
    {
        // Calling an action might indirectly modify the map, so we first
        // collect strong pointers to gateways into a local collection.
        //
        std::vector<detail::Gateway::Ptr> gateway_ptrs;
        gateway_ptrs.reserve(endpoint_to_gateway_.size());
        for (const auto& ep_to_gw : endpoint_to_gateway_)
        {
            const auto gateway_ptr = ep_to_gw.second.lock();
            if (gateway_ptr)
            {
                gateway_ptrs.push_back(gateway_ptr);
            }
        }

        for (const auto& gateway_ptr : gateway_ptrs)
        {
            action(*gateway_ptr);
        }
    }

    void onGatewaySubscription(const Endpoint endpoint)
    {
        if (is_connected_)
        {
            findRegisteredGateway(endpoint, [](auto& gateway, auto) {
                //
                gateway.event(detail::Gateway::Event::Connected{});
            });
        }
    }

    /// Unregisters the gateway associated with the given endpoint.
    ///
    /// Called on the gateway disposal (correspondingly on its channel destruction).
    /// The "dying" gateway might wish to notify the remote router about its disposal.
    /// The router fulfills the wish if the gateway was registered and the router is connected.
    ///
    void onGatewayDisposal(const Endpoint& endpoint, const bool send_ch_end)
    {
        const bool was_registered = (endpoint_to_gateway_.erase(endpoint) > 0);

        // Notify "remote" router about the gateway disposal (aka channel completion).
        // The router will propagate "ChEnd" event to the counterpart gateway (if it's registered).
        //
        if (was_registered && send_ch_end && isConnected(endpoint))
        {
            Route_1_0 route{&memory_};
            auto&     channel_end  = route.set_channel_end();
            channel_end.tag        = endpoint.getTag();
            channel_end.error_code = 0;  // No error b/c it's a normal channel completion.

            const int result = tryPerformOnSerialized(route, [this](const auto payload) {
                //
                return client_pipe_->send({{payload}});
            });
            // Best efforts strategy - gateway anyway is gone, so nowhere to report.
            (void) result;
        }
    }

    CETL_NODISCARD int handlePipeEvent(const pipe::ClientPipe::Event::Connected) const
    {
        // TODO: log client pipe connection

        Route_1_0 route{&memory_};
        auto&     route_conn     = route.set_connect();
        route_conn.version.major = VERSION_MAJOR;
        route_conn.version.minor = VERSION_MINOR;

        return tryPerformOnSerialized(route, [this](const auto payload) {
            //
            return client_pipe_->send({{payload}});
        });
    }

    CETL_NODISCARD int handlePipeEvent(const pipe::ClientPipe::Event::Message& msg)
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
                        [this](const RouteConnect_1_0& route_conn) {
                            //
                            handleRouteConnect(route_conn);
                        },
                        [this, msg_payload](const RouteChannelMsg_1_0& route_ch_msg) {
                            //
                            handleRouteChannelMsg(route_ch_msg, msg_payload);
                        },
                        [this](const RouteChannelEnd_1_0& route_ch_end) {
                            //
                            handleRouteChannelEnd(route_ch_end);
                        }),
                    route_msg.union_value);

        return 0;
    }

    CETL_NODISCARD int handlePipeEvent(const pipe::ClientPipe::Event::Disconnected)
    {
        if (is_connected_)
        {
            is_connected_ = false;

            // The whole router is disconnected, so we need to unregister and notify all gateways.
            //
            EndpointToWeakGateway local_gateways;
            std::swap(local_gateways, endpoint_to_gateway_);
            for (const auto& ep_to_gw : local_gateways)
            {
                const auto gateway = ep_to_gw.second.lock();
                if (gateway)
                {
                    gateway->event(detail::Gateway::Event::Completed{ErrorCode::Disconnected});
                }
            }
        }
        return 0;
    }

    void handleRouteConnect(const RouteConnect_1_0&)
    {
        if (!is_connected_)
        {
            is_connected_ = true;

            // We've got connection response from the server, so we need to notify all local gateways.
            //
            forEachRegisteredGateway([](auto& gateway) {
                //
                gateway.event(detail::Gateway::Event::Connected{});
            });
        }
    }

    void handleRouteChannelMsg(const RouteChannelMsg_1_0& route_ch_msg, const Payload payload)
    {
        const Endpoint endpoint{route_ch_msg.tag};

        const auto ep_to_gw = endpoint_to_gateway_.find(endpoint);
        if (ep_to_gw != endpoint_to_gateway_.end())
        {
            const auto gateway = ep_to_gw->second.lock();
            if (gateway)
            {
                gateway->event(detail::Gateway::Event::Message{route_ch_msg.sequence, payload});
                return;
            }
        }

        // TODO: log unsolicited message
    }

    void handleRouteChannelEnd(const RouteChannelEnd_1_0& route_ch_end)
    {
        const Endpoint endpoint{route_ch_end.tag};
        const auto     error_code = static_cast<ErrorCode>(route_ch_end.error_code);

        findRegisteredGateway(endpoint, [this, error_code](auto& gateway, auto it) {
            //
            endpoint_to_gateway_.erase(it);
            gateway.event(detail::Gateway::Event::Completed{error_code});
        });
    }

    cetl::pmr::memory_resource& memory_;
    pipe::ClientPipe::Ptr       client_pipe_;
    Endpoint::Tag               next_tag_;
    EndpointToWeakGateway       endpoint_to_gateway_;
    bool                        is_connected_;

};  // ClientRouterImpl

}  // namespace

CETL_NODISCARD ClientRouter::Ptr ClientRouter::make(cetl::pmr::memory_resource& memory,
                                                    pipe::ClientPipe::Ptr       client_pipe)
{
    return std::make_unique<ClientRouterImpl>(memory, std::move(client_pipe));
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
