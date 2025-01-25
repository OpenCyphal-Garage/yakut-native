//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_NET_SOCKET_CLIENT_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_NET_SOCKET_CLIENT_HPP_INCLUDED

#include "client_pipe.hpp"
#include "ipc/ipc_types.hpp"
#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "socket_base.hpp"

#include <cetl/cetl.hpp>
#include <libcyphal/executor.hpp>

#include <string>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

class NetSocketClient final : public SocketBase, public ClientPipe
{
public:
    NetSocketClient(libcyphal::IExecutor& executor, std::string server_ip, const int server_port);

    NetSocketClient(const NetSocketClient&)                = delete;
    NetSocketClient(NetSocketClient&&) noexcept            = delete;
    NetSocketClient& operator=(const NetSocketClient&)     = delete;
    NetSocketClient& operator=(NetSocketClient&&) noexcept = delete;

    ~NetSocketClient() override;

    // ClientPipe

    CETL_NODISCARD int start(EventHandler event_handler) override;

    CETL_NODISCARD int send(const Payloads payloads) override
    {
        return SocketBase::send(state_, payloads);
    }

private:
    void handle_connect();
    void handle_receive();
    void handle_disconnect();

    const std::string                        server_ip_;
    const int                                server_port_;
    platform::IPosixExecutorExtension* const posix_executor_ext_;
    State                                    state_;
    libcyphal::IExecutor::Callback::Any      socket_callback_;
    EventHandler                             event_handler_;

};  // NetSocketClient

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_NET_SOCKET_CLIENT_HPP_INCLUDED
