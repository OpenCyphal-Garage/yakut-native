//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_ENGINE_CYPHAL_ANY_TRANSPORT_BAG_HPP_INCLUDED
#define OCVSMD_DAEMON_ENGINE_CYPHAL_ANY_TRANSPORT_BAG_HPP_INCLUDED

#include <libcyphal/transport/transport.hpp>
#include <libcyphal/types.hpp>

#include <memory>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{
namespace cyphal
{

/// Represents storage of some (UDP, CAN) libcyphal transport and its media.
///
class AnyTransportBag
{
public:
    using Ptr       = std::unique_ptr<AnyTransportBag>;
    using Transport = libcyphal::transport::ITransport;

    AnyTransportBag(const AnyTransportBag&)                = delete;
    AnyTransportBag(AnyTransportBag&&) noexcept            = delete;
    AnyTransportBag& operator=(const AnyTransportBag&)     = delete;
    AnyTransportBag& operator=(AnyTransportBag&&) noexcept = delete;

    virtual ~AnyTransportBag() = default;

    virtual Transport& getTransport() const = 0;

protected:
    AnyTransportBag() = default;

};  // AnyTransportBag

}  // namespace cyphal
}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd

#endif  // OCVSMD_DAEMON_ENGINE_CYPHAL_ANY_TRANSPORT_BAG_HPP_INCLUDED
