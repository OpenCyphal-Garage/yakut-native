//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_ENGINE_CYPHAL_UDP_TRANSPORT_BAG_HPP_INCLUDED
#define OCVSMD_DAEMON_ENGINE_CYPHAL_UDP_TRANSPORT_BAG_HPP_INCLUDED

#include "config.hpp"
#include "platform/udp/udp_media.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/transport/errors.hpp>
#include <libcyphal/transport/udp/udp_transport.hpp>
#include <libcyphal/transport/udp/udp_transport_impl.hpp>
#include <libcyphal/types.hpp>

#include <cstddef>
#include <string>
#include <utility>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{
namespace cyphal
{

/// Holds (internally) instance of the UDP transport and its media (if any).
///
struct UdpTransportBag final
{
    UdpTransportBag(cetl::pmr::memory_resource& memory, libcyphal::IExecutor& executor)
        : memory_{memory}
        , executor_{executor}
        , media_collection_{memory, executor, memory}
    {
    }

    libcyphal::transport::udp::IUdpTransport* create(const Config::Ptr& config)
    {
        CETL_DEBUG_ASSERT(config, "");

        std::string udp_ifaces;
        for (const auto& iface : config->getCyphalTransportInterfaces())
        {
            const static std::string udp_prefix = "udp:";
            if (0 == iface.find(udp_prefix))
            {
                udp_ifaces += iface.substr(udp_prefix.size());
                udp_ifaces += ",";
            }
        }

        media_collection_.parse(udp_ifaces);
        auto maybe_udp_transport = makeTransport({memory_}, executor_, media_collection_.span(), TxQueueCapacity);
        if (const auto* failure = cetl::get_if<libcyphal::transport::FactoryFailure>(&maybe_udp_transport))
        {
            (void) failure;
            return nullptr;
        }
        transport_ = cetl::get<libcyphal::UniquePtr<libcyphal::transport::udp::IUdpTransport>>(  //
            std::move(maybe_udp_transport));

        if (const auto node_id = config->getCyphalAppNodeId())
        {
            transport_->setLocalNodeId(node_id.value());
        }

        // transport_->setTransientErrorHandler(platform::CommonHelpers::Udp::transientErrorReporter);

        return transport_.get();
    }

private:
    static constexpr std::size_t TxQueueCapacity = 16;

    cetl::pmr::memory_resource&                                    memory_;
    libcyphal::IExecutor&                                          executor_;
    platform::udp::UdpMediaCollection                              media_collection_;
    libcyphal::UniquePtr<libcyphal::transport::udp::IUdpTransport> transport_{nullptr};

};  // UdpTransportBag

}  // namespace cyphal
}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd

#endif  // OCVSMD_DAEMON_ENGINE_CYPHAL_UDP_TRANSPORT_BAG_HPP_INCLUDED
