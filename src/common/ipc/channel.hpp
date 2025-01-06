//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_CHANNEL_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_CHANNEL_HPP_INCLUDED

#include "dsdl_helpers.hpp"
#include "gateway.hpp"

#include <nunavut/support/serialization.hpp>

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

template <typename Input_, typename Output_>
class Channel final : public AnyChannel
{
public:
    using Input        = Input_;
    using Output       = Output_;
    using EventVar     = EventVar<Input>;
    using EventHandler = EventHandler<Input>;

    Channel(Channel&& other) noexcept
        : memory_{other.memory_}
        , gateway_{std::move(other.gateway_)}
        , event_handler_{std::move(other.event_handler_)}
    {
        setupEventHandler();
    }

    Channel(const Channel&)                      = delete;
    Channel& operator=(const Channel&)           = delete;
    Channel& operator=(Channel&& other) noexcept = delete;

    ~Channel()
    {
        gateway_->setEventHandler(nullptr);
        event_handler_ = nullptr;
    }

    using SendFailure = nunavut::support::Error;
    using SendResult  = cetl::optional<SendFailure>;

    SendResult send(const Output& output)
    {
        constexpr std::size_t BufferSize = Output::_traits_::SerializationBufferSizeBytes;
        constexpr bool        IsOnStack  = BufferSize <= MsgSmallPayloadSize;

        return tryPerformOnSerialized<Output, SendResult, BufferSize, IsOnStack>(  //
            output,
            [this](const auto payload) {
                //
                gateway_->send(payload);
                return cetl::nullopt;
            });
    }

private:
    friend class ClientRouter;

    Channel(cetl::pmr::memory_resource& memory, detail::Gateway::Ptr gateway, EventHandler event_handler)
        : memory_{memory}
        , gateway_{std::move(gateway)}
        , event_handler_{std::move(event_handler)}
    {
        CETL_DEBUG_ASSERT(gateway_, "");
        CETL_DEBUG_ASSERT(event_handler_, "");

        setupEventHandler();
    }

    void setupEventHandler()
    {
        gateway_->setEventHandler([this](const detail::Gateway::Event::Var& gateway_event_var) {
            //
            cetl::visit(
                [this](const auto& gateway_event) {
                    //
                    handleGatewayEvent(gateway_event);
                },
                gateway_event_var);
        });
    }

    void handleGatewayEvent(const detail::Gateway::Event::Message& gateway_message)
    {
        Input input{&memory_};
        if (tryDeserializePayload(gateway_message.payload, input))
        {
            event_handler_(input);
        }
    }

    void handleGatewayEvent(const detail::Gateway::Event::Connected)
    {
        event_handler_(Connected{});
    }

    void handleGatewayEvent(const detail::Gateway::Event::Disconnected)
    {
        event_handler_(Disconnected{});
    }

    static constexpr std::size_t MsgSmallPayloadSize = 256;

    cetl::pmr::memory_resource& memory_;
    detail::Gateway::Ptr        gateway_;
    EventHandler                event_handler_;

};  // Channel

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_CHANNEL_HPP_INCLUDED
