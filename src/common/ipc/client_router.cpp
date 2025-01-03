//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "client_router.hpp"

#include "gateway.hpp"
#include "pipe/client_pipe.hpp"

#include <cetl/cetl.hpp>

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

class ClientRouterImpl final : public ClientRouter
{
public:
    explicit ClientRouterImpl(pipe::ClientPipe::Ptr client_pipe)
        : client_pipe_{std::move(client_pipe)}
        , next_tag_{0}
    {
        CETL_DEBUG_ASSERT(client_pipe_, "");
    }

    // ClientRouter

    CETL_NODISCARD detail::Gateway::Ptr makeGateway() override
    {
        const Tag new_tag        = ++next_tag_;
        auto      gateway        = GatewayImpl::create(new_tag, *this);
        tag_to_gateway_[new_tag] = gateway;
        return gateway;
    }

private:
    using Tag = std::uint64_t;

    class GatewayImpl final : public std::enable_shared_from_this<GatewayImpl>, public detail::Gateway
    {
        struct Private
        {
            explicit Private() = default;
        };

    public:
        static std::shared_ptr<GatewayImpl> create(const Tag tag, ClientRouterImpl& router)
        {
            return std::make_shared<GatewayImpl>(Private(), tag, router);
        }

        GatewayImpl(Private, const Tag tag, ClientRouterImpl& router)
            : tag_{tag}
            , router_{router}
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

        void send(const Payload payload) override
        {
            router_.client_pipe_->sendMessage(payload);
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
        ClientRouterImpl& router_;
        EventHandler      event_handler_;

    };  // GatewayImpl

    pipe::ClientPipe::Ptr                         client_pipe_;
    Tag                                           next_tag_;
    std::unordered_map<Tag, detail::Gateway::Ptr> tag_to_gateway_;

};  // ClientRouterImpl

}  // namespace

ClientRouter::Ptr ClientRouter::make(pipe::ClientPipe::Ptr client_pipe)
{
    return std::make_unique<ClientRouterImpl>(std::move(client_pipe));
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
