//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_CLIENT_ROUTER_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_CLIENT_ROUTER_HPP_INCLUDED

#include "channel.hpp"
#include "gateway.hpp"
#include "pipe/client_pipe.hpp"

#include <cetl/cetl.hpp>

#include <memory>

namespace ocvsmd
{
namespace common
{
namespace ipc
{

class ClientRouter
{
public:
    using Ptr = std::unique_ptr<ClientRouter>;

    static Ptr make(pipe::ClientPipe::Ptr client_pipe);

    ClientRouter(const ClientRouter&)                = delete;
    ClientRouter(ClientRouter&&) noexcept            = delete;
    ClientRouter& operator=(const ClientRouter&)     = delete;
    ClientRouter& operator=(ClientRouter&&) noexcept = delete;

    virtual ~ClientRouter() = default;

    template <typename Input, typename Output>
    CETL_NODISCARD Channel<Input, Output> makeChannel(AnyChannel::EventHandler<Input> event_handler)
    {
        return Channel<Input, Output>{makeGateway(), event_handler};
    }

protected:
    ClientRouter() = default;

    CETL_NODISCARD virtual detail::Gateway::Ptr makeGateway() = 0;

};  // ClientRouter

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_CLIENT_ROUTER_HPP_INCLUDED
