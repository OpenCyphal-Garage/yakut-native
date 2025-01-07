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
#include <libcyphal/common/crc.hpp>

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

    template <typename Message>
    using EventVar = cetl::variant<Message, Connected, Disconnected>;

    template <typename Message>
    using EventHandler = std::function<void(const EventVar<Message>&)>;

    /// Builds a service ID from either the service name (if not empty), or message type name.
    ///
    template <typename Message>
    static detail::ServiceId getServiceId(const cetl::string_view service_name) noexcept
    {
        const cetl::string_view srv_or_msg_name = !service_name.empty()  //
                                                      ? service_name
                                                      : Message::_traits_::FullNameAndVersion();

        const libcyphal::common::CRC64WE crc64{srv_or_msg_name.cbegin(), srv_or_msg_name.cend()};
        return crc64.get();
    }

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
        , service_id_{other.service_id_}
        , event_handler_{std::move(other.event_handler_)}
    {
        setupEventHandling();
    }

    ~Channel() = default;

    Channel(const Channel&)                      = delete;
    Channel& operator=(const Channel&)           = delete;
    Channel& operator=(Channel&& other) noexcept = delete;

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
                gateway_->send(service_id_, payload);
                return cetl::nullopt;
            });
    }

    Channel& setEventHandler(EventHandler event_handler)
    {
        event_handler_ = std::move(event_handler);
        setupEventHandling();
        return *this;
    }

private:
    friend class ClientRouter;
    friend class ServerRouter;

    Channel(cetl::pmr::memory_resource& memory, detail::Gateway::Ptr gateway, const detail::ServiceId service_id)
        : memory_{memory}
        , gateway_{std::move(gateway)}
        , service_id_{service_id}
    {
        CETL_DEBUG_ASSERT(gateway_, "");
    }

    void setupEventHandling()
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
        if (event_handler_)
        {
            Input input{&memory_};
            if (tryDeserializePayload(gateway_message.payload, input))
            {
                event_handler_(input);
            }
        }
    }

    void handleGatewayEvent(const detail::Gateway::Event::Connected)
    {
        if (event_handler_)
        {
            event_handler_(Connected{});
        }
    }

    void handleGatewayEvent(const detail::Gateway::Event::Disconnected)
    {
        if (event_handler_)
        {
            event_handler_(Disconnected{});
        }
    }

    static constexpr std::size_t MsgSmallPayloadSize = 256;

    cetl::pmr::memory_resource& memory_;
    detail::Gateway::Ptr        gateway_;
    detail::ServiceId           service_id_;
    EventHandler                event_handler_;

};  // Channel

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_CHANNEL_HPP_INCLUDED
