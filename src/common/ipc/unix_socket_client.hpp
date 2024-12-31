//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_UNIX_SOCKET_CLIENT_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_UNIX_SOCKET_CLIENT_HPP_INCLUDED

#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "unix_socket_base.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <libcyphal/executor.hpp>

#include <functional>
#include <string>

namespace ocvsmd
{
namespace common
{
namespace ipc
{

class UnixSocketClient final : public UnixSocketBase
{
public:
    struct ServerEvent
    {
        struct Message
        {
            cetl::span<const std::uint8_t> payload;
        };
        struct Connected
        {};
        struct Disconnected
        {};

        using Var = cetl::variant<Message, Connected, Disconnected>;

    };  // ServerEvent

    UnixSocketClient(libcyphal::IExecutor& executor, std::string socket_path);

    UnixSocketClient(UnixSocketClient&&)                 = delete;
    UnixSocketClient(const UnixSocketClient&)            = delete;
    UnixSocketClient& operator=(UnixSocketClient&&)      = delete;
    UnixSocketClient& operator=(const UnixSocketClient&) = delete;

    ~UnixSocketClient();

    int start(std::function<int(const ServerEvent::Var&)>&& server_event_handler);

    int sendMessage(const cetl::span<const std::uint8_t> payload) const
    {
        return UnixSocketBase::sendMessage(client_fd_, payload);
    }

private:
    void handle_socket();

    std::string                                 socket_path_;
    int                                         client_fd_;
    platform::IPosixExecutorExtension* const    posix_executor_ext_;
    libcyphal::IExecutor::Callback::Any         socket_callback_;
    std::function<int(const ServerEvent::Var&)> server_event_handler_;

};  // UnixSocketClient

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_UNIX_SOCKET_CLIENT_HPP_INCLUDED
