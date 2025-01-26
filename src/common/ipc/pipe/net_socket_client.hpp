//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_NET_SOCKET_CLIENT_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_NET_SOCKET_CLIENT_HPP_INCLUDED

#include "client_pipe.hpp"
#include "socket_client_base.hpp"

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

class NetSocketClient final : public SocketClientBase
{
public:
    NetSocketClient(libcyphal::IExecutor& executor, std::string server_ip, const int server_port);

    NetSocketClient(const NetSocketClient&)                = delete;
    NetSocketClient(NetSocketClient&&) noexcept            = delete;
    NetSocketClient& operator=(const NetSocketClient&)     = delete;
    NetSocketClient& operator=(NetSocketClient&&) noexcept = delete;

    ~NetSocketClient() override = default;

private:
    using Base = SocketClientBase;

    CETL_NODISCARD int makeSocketHandle(int& out_fd) override;

    const std::string server_ip_;
    const int         server_port_;

};  // NetSocketClient

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_NET_SOCKET_CLIENT_HPP_INCLUDED
