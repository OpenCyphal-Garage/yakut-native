//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_CHANNEL_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_CHANNEL_HPP_INCLUDED

#include "dsdl_helpers.hpp"
#include "gateway.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <cstddef>
#include <functional>
#include <utility>

namespace ocvsmd
{
namespace common
{
namespace ipc
{

class AnyChannel
{
public:
    struct Connected
    {};

    struct Disconnected
    {};

    template <typename Input>
    using EventVar = cetl::variant<Input, Connected, Disconnected>;

    template <typename Input>
    using EventHandler = std::function<void(const EventVar<Input>&)>;

protected:
    AnyChannel() = default;

};  // AnyChannel

template <typename Input, typename Output>
class Channel final : public AnyChannel
{
public:
    Channel(Channel&& other) noexcept
        : gateway_{std::move(other.gateway_)}
        , event_handler_{std::move(other.event_handler_)}
    {
        setupEventHandler();
    }

    Channel& operator=(Channel&& other) noexcept
    {
        if (this != &other)
        {
            gateway_       = std::move(other.gateway_);
            event_handler_ = std::move(other.event_handler_);
            setupEventHandler();
        }
        return *this;
    }

    Channel(const Channel&)            = delete;
    Channel& operator=(const Channel&) = delete;

    ~Channel()
    {
        gateway_->setEventHandler(nullptr);
        event_handler_ = nullptr;
    }

    void send(const Output& output)
    {
        constexpr std::size_t BufferSize = Output::;
        constexpr bool        IsOnStack  = BufferSize <= MsgSmallPayloadSize;

        return tryPerformOnSerialized<Output, Result, BufferSize, IsOnStack>(  //
            output,
            [this](const auto payload) {
                //
                gateway_->send(payload);
            });
    }

private:
    friend class ClientRouter;

    Channel(detail::Gateway::Ptr gateway, EventHandler<Input> event_handler)
        : gateway_{std::move(gateway)}
        , event_handler_{std::move(event_handler)}
    {
        CETL_DEBUG_ASSERT(gateway_, "");
        CETL_DEBUG_ASSERT(event_handler_, "");

        setupEventHandler();
    }

    void setupEventHandler()
    {
        gateway_->setEventHandler([this](const auto& event) {
            //
            event_handler_(event);
        });
    }

    static constexpr std::size_t MsgSmallPayloadSize = 256;

    detail::Gateway::Ptr gateway_;
    EventHandler<Input>  event_handler_;

};  // Channel

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_CHANNEL_HPP_INCLUDED
