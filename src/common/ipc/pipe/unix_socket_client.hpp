//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_CLIENT_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_CLIENT_HPP_INCLUDED

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

class UnixSocketClient final : public SocketClientBase
{
public:
    UnixSocketClient(libcyphal::IExecutor& executor, std::string socket_path);

    UnixSocketClient(const UnixSocketClient&)                = delete;
    UnixSocketClient(UnixSocketClient&&) noexcept            = delete;
    UnixSocketClient& operator=(const UnixSocketClient&)     = delete;
    UnixSocketClient& operator=(UnixSocketClient&&) noexcept = delete;

    ~UnixSocketClient() override = default;

private:
    using Base = SocketClientBase;

    CETL_NODISCARD int makeSocketHandle(int& out_fd) override;

    const std::string socket_path_;

};  // UnixSocketClient

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_CLIENT_HPP_INCLUDED
