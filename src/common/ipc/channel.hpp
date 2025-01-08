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
    struct Connected final
    {};

    struct Disconnected final
    {};

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
    using Input  = Input_;
    using Output = Output_;

    using EventVar     = cetl::variant<Input, Connected, Disconnected>;
    using EventHandler = std::function<void(const EventVar&)>;

    Channel(Channel&& other) noexcept
        : memory_{other.memory_}
        , gateway_{std::move(other.gateway_)}
        , service_id_{other.service_id_}
    {
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

    void subscribe(EventHandler event_handler)
    {
        if (event_handler)
        {
            gateway_->subscribe(
                [adapter = Adapter{memory_, std::move(event_handler)}](const GatewayEvent::Var& ge_var) {
                    //
                    cetl::visit(adapter, ge_var);
                });
        }
        else
        {
            gateway_->subscribe(nullptr);
        }
    }

private:
    friend class ClientRouter;
    friend class ServerRouter;

    using GatewayEvent = detail::Gateway::Event;

    struct Adapter final
    {
        cetl::pmr::memory_resource& memory;            // NOLINT
        EventHandler                ch_event_handler;  // NOLINT

        void operator()(const GatewayEvent::Connected&) const
        {
            ch_event_handler(Connected{});
        }

        void operator()(const GatewayEvent::Message& gateway_msg) const
        {
            Input input{&memory};
            if (tryDeserializePayload(gateway_msg.payload, input))
            {
                ch_event_handler(input);
            }
        }

        void operator()(const GatewayEvent::Disconnected&) const
        {
            ch_event_handler(Disconnected{});
        }

    };  // Adapter

    Channel(cetl::pmr::memory_resource& memory, detail::Gateway::Ptr gateway, const detail::ServiceId service_id)
        : memory_{memory}
        , gateway_{std::move(gateway)}
        , service_id_{service_id}
    {
        CETL_DEBUG_ASSERT(gateway_, "");
    }

    static constexpr std::size_t MsgSmallPayloadSize = 256;

    cetl::pmr::memory_resource& memory_;
    detail::Gateway::Ptr        gateway_;
    detail::ServiceId           service_id_;

};  // Channel

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_CHANNEL_HPP_INCLUDED
