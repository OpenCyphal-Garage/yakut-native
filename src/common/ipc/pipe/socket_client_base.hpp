//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_SOCKET_CLIENT_BASE_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_SOCKET_CLIENT_BASE_HPP_INCLUDED

#include "client_pipe.hpp"
#include "ipc/ipc_types.hpp"
#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "socket_base.hpp"

#include <cetl/cetl.hpp>
#include <libcyphal/executor.hpp>

#include <cstddef>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

class SocketClientBase : public SocketBase, public ClientPipe
{
public:
    SocketClientBase(const SocketClientBase&)                = delete;
    SocketClientBase(SocketClientBase&&) noexcept            = delete;
    SocketClientBase& operator=(const SocketClientBase&)     = delete;
    SocketClientBase& operator=(SocketClientBase&&) noexcept = delete;

    ~SocketClientBase() override;

protected:
    explicit SocketClientBase(libcyphal::IExecutor& executor);

    // ClientPipe
    CETL_NODISCARD int start(EventHandler event_handler) override;
    CETL_NODISCARD int send(const Payloads payloads) override;

    CETL_NODISCARD virtual int makeSocketHandle(int& out_fd) = 0;

    CETL_NODISCARD int connectSocket(const int fd, const void* const addr_ptr, const std::size_t addr_size) const;

private:
    void handle_connect();
    void handle_receive();
    void handle_disconnect();

    platform::IPosixExecutorExtension* const posix_executor_ext_;
    State                                    state_;
    libcyphal::IExecutor::Callback::Any      socket_callback_;
    EventHandler                             event_handler_;

};  // SocketClientBase

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_SOCKET_CLIENT_BASE_HPP_INCLUDED
