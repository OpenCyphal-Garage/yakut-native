//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "unix_socket_server.hpp"

#include "platform/posix_executor_extension.hpp"

#include <cetl/cetl.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
UnixSocketServer::UnixSocketServer(libcyphal::IExecutor& executor, std::string socket_path)
    : executor_{executor}
    , socket_path_{std::move(socket_path)}
    , server_fd_{-1}
{
}

UnixSocketServer::~UnixSocketServer()
{
    if (server_fd_ != -1)
    {
        ::close(server_fd_);
        ::unlink(socket_path_.c_str());
    }
}

bool UnixSocketServer::start()
{
    server_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ == -1)
    {
        std::cerr << "Failed to create socket: " << ::strerror(errno) << "\n";
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
    ::unlink(socket_path_.c_str());

    if (::bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        std::cerr << "Failed to bind socket: " << ::strerror(errno) << "\n";
        return false;
    }

    if (::listen(server_fd_, 5) == -1)
    {
        std::cerr << "Failed to listen on socket: " << ::strerror(errno) << "\n";
        return false;
    }

    return true;
}

void UnixSocketServer::accept()
{
    int client_fd = ::accept(server_fd_, nullptr, nullptr);
    if (client_fd == -1)
    {
        std::cerr << "Failed to accept connection: " << ::strerror(errno) << "\n";
        return;
    }

    handle_client(client_fd);
    ::close(client_fd);
}

void UnixSocketServer::handle_client(int client_fd)
{
    char    buffer[256];
    ssize_t bytes_read = ::read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        std::cout << "Received: " << buffer << "\n";
        ::write(client_fd, buffer, bytes_read);  // Echo back
    }
}

CETL_NODISCARD libcyphal::IExecutor::Callback::Any UnixSocketServer::registerListenCallback(
    libcyphal::IExecutor::Callback::Function&& function) const
{
    auto* const posix_executor_ext = cetl::rtti_cast<platform::IPosixExecutorExtension*>(&executor_);
    if (nullptr == posix_executor_ext)
    {
        return {};
    }

    CETL_DEBUG_ASSERT(udp_handle_.fd >= 0, "");
    return posix_executor_ext->registerAwaitableCallback(  //
        std::move(function),
        platform::IPosixExecutorExtension::Trigger::Readable{server_fd_});
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
