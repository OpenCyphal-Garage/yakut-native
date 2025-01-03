//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_CLIENT_PIPE_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_CLIENT_PIPE_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

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

class ClientPipe
{
public:
    using Ptr = std::unique_ptr<ClientPipe>;

    using Payload = cetl::span<const std::uint8_t>;

    struct Event
    {
        struct Connected
        {};
        struct Disconnected
        {};
        struct Message
        {
            Payload payload;

        };  // Message

        using Var = cetl::variant<Message, Connected, Disconnected>;

    };  // Event

    using EventHandler = std::function<int(const Event::Var&)>;

    ClientPipe(const ClientPipe&)                = delete;
    ClientPipe(ClientPipe&&) noexcept            = delete;
    ClientPipe& operator=(const ClientPipe&)     = delete;
    ClientPipe& operator=(ClientPipe&&) noexcept = delete;

    virtual ~ClientPipe() = default;

    virtual int start(EventHandler event_handler)  = 0;
    virtual int sendMessage(const Payload payload) = 0;

protected:
    ClientPipe() = default;

};  // ClientPipe

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_CLIENT_PIPE_HPP_INCLUDED
