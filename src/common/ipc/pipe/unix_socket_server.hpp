//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_SERVER_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_SERVER_HPP_INCLUDED

#include "socket_server_base.hpp"

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

class UnixSocketServer final : public SocketServerBase
{
public:
    UnixSocketServer(libcyphal::IExecutor& executor, std::string socket_path);

    UnixSocketServer(UnixSocketServer&&)                 = delete;
    UnixSocketServer(const UnixSocketServer&)            = delete;
    UnixSocketServer& operator=(UnixSocketServer&&)      = delete;
    UnixSocketServer& operator=(const UnixSocketServer&) = delete;

    ~UnixSocketServer() override = default;

private:
    using Base = SocketServerBase;

    CETL_NODISCARD int makeSocketHandle(int& out_fd) override;

    const std::string socket_path_;

};  // UnixSocketServer

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_SERVER_HPP_INCLUDED
