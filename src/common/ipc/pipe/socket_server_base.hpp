//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_SOCKET_SERVER_BASE_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_SOCKET_SERVER_BASE_HPP_INCLUDED

#include "client_context.hpp"
#include "ipc/ipc_types.hpp"
#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "server_pipe.hpp"
#include "socket_base.hpp"

#include <cetl/cetl.hpp>
#include <libcyphal/executor.hpp>

#include <unordered_map>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

class SocketServerBase : public SocketBase, public ServerPipe
{
public:
    SocketServerBase(const SocketServerBase&)                = delete;
    SocketServerBase(SocketServerBase&&) noexcept            = delete;
    SocketServerBase& operator=(const SocketServerBase&)     = delete;
    SocketServerBase& operator=(SocketServerBase&&) noexcept = delete;

protected:
    explicit SocketServerBase(libcyphal::IExecutor& executor);
    ~SocketServerBase() override;

    // ServerPipe
    //
    CETL_NODISCARD int start(EventHandler event_handler) override;
    CETL_NODISCARD int send(const ClientId client_id, const Payloads payloads) override;

    CETL_NODISCARD virtual int makeSocketHandle(int& out_fd) = 0;

private:
    void           handleAccept();
    void           handleClientRequest(const ClientId client_id);
    ClientContext* tryFindClientContext(const ClientId client_id);

    int                                              server_fd_;
    platform::IPosixExecutorExtension* const         posix_executor_ext_;
    ClientId                                         unique_client_id_counter_;
    EventHandler                                     event_handler_;
    libcyphal::IExecutor::Callback::Any              accept_callback_;
    std::unordered_map<ClientId, ClientContext::Ptr> client_id_to_context_;

};  // SocketServerBase

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_SOCKET_SERVER_BASE_HPP_INCLUDED
