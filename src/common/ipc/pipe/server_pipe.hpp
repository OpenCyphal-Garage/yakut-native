//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_SERVER_PIPE_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_SERVER_PIPE_HPP_INCLUDED

#include "pipe_types.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

class ServerPipe
{
public:
    using Ptr = std::unique_ptr<ServerPipe>;

    using ClientId = std::size_t;

    struct Event
    {
        struct Connected
        {
            ClientId client_id;
        };
        struct Disconnected
        {
            ClientId client_id;
        };
        struct Message
        {
            ClientId client_id;
            Payload  payload;

        };  // Message

        using Var = cetl::variant<Message, Connected, Disconnected>;

    };  // Event

    using EventHandler = std::function<int(const Event::Var&)>;

    ServerPipe(const ServerPipe&)                = delete;
    ServerPipe(ServerPipe&&) noexcept            = delete;
    ServerPipe& operator=(const ServerPipe&)     = delete;
    ServerPipe& operator=(ServerPipe&&) noexcept = delete;

    virtual ~ServerPipe() = default;

    virtual int start(EventHandler event_handler)                              = 0;
    virtual int sendMessage(const ClientId client_id, const Payloads payloads) = 0;

protected:
    ServerPipe() = default;

};  // ServerPipe

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_SERVER_PIPE_HPP_INCLUDED
