//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_NET_SOCKET_SERVER_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_NET_SOCKET_SERVER_HPP_INCLUDED

#include "socket_server_base.hpp"

#include <cetl/cetl.hpp>
#include <libcyphal/executor.hpp>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

class NetSocketServer final : public SocketServerBase
{
public:
    NetSocketServer(libcyphal::IExecutor& executor, const int server_port);

    NetSocketServer(const NetSocketServer&)                = delete;
    NetSocketServer(NetSocketServer&&) noexcept            = delete;
    NetSocketServer& operator=(const NetSocketServer&)     = delete;
    NetSocketServer& operator=(NetSocketServer&&) noexcept = delete;

    ~NetSocketServer() override = default;

private:
    using Base = SocketServerBase;

    CETL_NODISCARD int makeSocketHandle(int& out_fd) override;

    const int server_port_;

};  // NetSocketServer

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_NET_SOCKET_SERVER_HPP_INCLUDED
