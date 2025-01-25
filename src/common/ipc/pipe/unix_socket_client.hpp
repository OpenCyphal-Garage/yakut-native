//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_CLIENT_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_CLIENT_HPP_INCLUDED

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

class UnixSocketClient final : public SocketBase, public ClientPipe
{
public:
    UnixSocketClient(libcyphal::IExecutor& executor, std::string socket_path);

    UnixSocketClient(const UnixSocketClient&)                = delete;
    UnixSocketClient(UnixSocketClient&&) noexcept            = delete;
    UnixSocketClient& operator=(const UnixSocketClient&)     = delete;
    UnixSocketClient& operator=(UnixSocketClient&&) noexcept = delete;

    ~UnixSocketClient() override;

    // ClientPipe

    CETL_NODISCARD int start(EventHandler event_handler) override;

    CETL_NODISCARD int send(const Payloads payloads) override
    {
        return SocketBase::send(state_, payloads);
    }

private:
    void handle_socket();

    const std::string                        socket_path_;
    platform::IPosixExecutorExtension* const posix_executor_ext_;
    State                                    state_;
    libcyphal::IExecutor::Callback::Any      socket_callback_;
    EventHandler                             event_handler_;

};  // UnixSocketClient

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_CLIENT_HPP_INCLUDED
