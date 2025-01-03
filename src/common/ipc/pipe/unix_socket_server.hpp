//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_SERVER_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_SERVER_HPP_INCLUDED

#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "server_pipe.hpp"
#include "unix_socket_base.hpp"

#include <libcyphal/executor.hpp>

#include <cerrno>
#include <memory>
#include <string>
#include <unordered_map>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{
namespace detail
{

class ClientContext
{
public:
    using Ptr = std::unique_ptr<ClientContext>;

    ClientContext() = default;

    ClientContext(const ClientContext&)                = delete;
    ClientContext(ClientContext&&) noexcept            = delete;
    ClientContext& operator=(const ClientContext&)     = delete;
    ClientContext& operator=(ClientContext&&) noexcept = delete;

    virtual ~ClientContext() = default;

};  // ClientContext

}  // namespace detail

class UnixSocketServer final : public UnixSocketBase, public ServerPipe
{
public:
    UnixSocketServer(libcyphal::IExecutor& executor, std::string socket_path);

    UnixSocketServer(UnixSocketServer&&)                 = delete;
    UnixSocketServer(const UnixSocketServer&)            = delete;
    UnixSocketServer& operator=(UnixSocketServer&&)      = delete;
    UnixSocketServer& operator=(const UnixSocketServer&) = delete;

    ~UnixSocketServer() override;

    int start(EventHandler event_handler) override;

    int sendMessage(const ClientId client_id, const Payload payload) override
    {
        const auto id_and_fd = client_id_to_fd_.find(client_id);
        if (id_and_fd == client_id_to_fd_.end())
        {
            return EINVAL;
        }
        return UnixSocketBase::sendMessage(id_and_fd->second, payload);
    }

private:
    void handle_accept();
    void handle_client_connection(const int client_fd);
    void handle_client_request(const ClientId client_id, const int client_fd);

    const std::string                                   socket_path_;
    int                                                 server_fd_;
    platform::IPosixExecutorExtension* const            posix_executor_ext_;
    ClientId                                            unique_client_id_counter_;
    EventHandler                                        event_handler_;
    std::unordered_map<int, detail::ClientContext::Ptr> client_fd_to_context_;
    std::unordered_map<ClientId, int>                   client_id_to_fd_;
    libcyphal::IExecutor::Callback::Any                 accept_callback_;

};  // UnixSocketServer

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_SERVER_HPP_INCLUDED
