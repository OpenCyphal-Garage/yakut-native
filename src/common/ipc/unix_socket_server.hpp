//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_UNIX_SOCKET_SERVER_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_UNIX_SOCKET_SERVER_HPP_INCLUDED

#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "unix_socket_base.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>
#include <libcyphal/executor.hpp>

#include <cerrno>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace detail
{

class ClientContext
{
public:
    ClientContext() = default;

    ClientContext(ClientContext&&)                 = delete;
    ClientContext(const ClientContext&)            = delete;
    ClientContext& operator=(ClientContext&&)      = delete;
    ClientContext& operator=(const ClientContext&) = delete;

    virtual ~ClientContext() = default;

};  // ClientContext

}  // namespace detail

class UnixSocketServer final : public UnixSocketBase
{
public:
    using ClientId = std::size_t;

    struct ClientEvent
    {
        struct Message
        {
            ClientId                       client_id;
            cetl::span<const std::uint8_t> payload;
        };
        struct Connected
        {
            ClientId client_id;
        };
        struct Disconnected
        {
            ClientId client_id;
        };

        using Var = cetl::variant<Message, Connected, Disconnected>;

    };  // ClientEvent

    UnixSocketServer(libcyphal::IExecutor& executor, std::string socket_path);

    UnixSocketServer(UnixSocketServer&&)                 = delete;
    UnixSocketServer(const UnixSocketServer&)            = delete;
    UnixSocketServer& operator=(UnixSocketServer&&)      = delete;
    UnixSocketServer& operator=(const UnixSocketServer&) = delete;

    ~UnixSocketServer();

    int start(std::function<int(const ClientEvent::Var&)>&& client_event_handler);

    int sendMessage(const ClientId client_id, const cetl::span<const std::uint8_t> payload) const
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

    const std::string                                               socket_path_;
    int                                                             server_fd_;
    platform::IPosixExecutorExtension* const                        posix_executor_ext_;
    ClientId                                                        unique_client_id_counter_;
    std::function<int(const ClientEvent::Var&)>                     client_event_handler_;
    std::unordered_map<int, std::unique_ptr<detail::ClientContext>> client_fd_to_context_;
    std::unordered_map<ClientId, int>                               client_id_to_fd_;
    libcyphal::IExecutor::Callback::Any                             accept_callback_;

};  // UnixSocketServer

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_UNIX_SOCKET_SERVER_HPP_INCLUDED
