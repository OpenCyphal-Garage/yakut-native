//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "unix_socket_server.hpp"

#include "platform/posix_executor_extension.hpp"
#include "platform/posix_utils.hpp"

#include <cetl/cetl.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>

#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace
{

constexpr int MaxConnections = 5;

}  // namespace

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
        platform::posixSyscallError([this] {
            //
            return ::close(server_fd_);
        });
        platform::posixSyscallError([this] {
            //
            return ::unlink(socket_path_.c_str());
        });
    }
}

bool UnixSocketServer::start()
{
    CETL_DEBUG_ASSERT(server_fd_ == -1, "");

    if (const auto err = platform::posixSyscallError([this] {
            //
            return server_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        }))
    {
        std::cerr << "Failed to create socket: " << ::strerror(err) << "\n";
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    ::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    platform::posixSyscallError([this] {
        //
        return ::unlink(socket_path_.c_str());
    });

    if (const auto err = platform::posixSyscallError([this, &addr] {
            //
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return ::bind(server_fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        }))
    {
        std::cerr << "Failed to bind socket: " << ::strerror(err) << "\n";
        return false;
    }

    if (const auto err = platform::posixSyscallError([this] {
            //
            return ::listen(server_fd_, MaxConnections);
        }))
    {
        std::cerr << "Failed to listen on socket: " << ::strerror(err) << "\n";
        return false;
    }

    return true;
}

void UnixSocketServer::accept() const
{
    CETL_DEBUG_ASSERT(server_fd_ != -1, "");

    int client_fd = -1;
    if (const auto err = platform::posixSyscallError([this, &client_fd] {
            //
            return client_fd = ::accept(server_fd_, nullptr, nullptr);
        }))
    {
        std::cerr << "Failed to accept connection: " << ::strerror(err) << "\n";
        return;
    }

    handle_client(client_fd);

    platform::posixSyscallError([this, client_fd] {
        //
        return ::close(client_fd);
    });
}

void UnixSocketServer::handle_client(const int client_fd)
{
    CETL_DEBUG_ASSERT(client_fd != -1, "");

    constexpr std::size_t      buf_size = 256;
    std::array<char, buf_size> buffer{};
    ssize_t                    bytes_read = 0;
    if (const auto err = platform::posixSyscallError([client_fd, &bytes_read, &buffer] {
            //
            return bytes_read = ::read(client_fd, buffer.data(), buffer.size() - 1);
        }))
    {
        std::cerr << "Failed to read: " << ::strerror(err) << "\n";
        return;
    }
    if (bytes_read == 0)
    {
        return;
    }
    buffer[bytes_read] = '\0';  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    std::cout << "Received: " << buffer.data() << "\n";

    // Echo back
    //
    if (const auto err = platform::posixSyscallError([client_fd, bytes_read, &buffer] {
            //
            return ::write(client_fd, buffer.data(), bytes_read);
        }))
    {
        std::cerr << "Failed to write: " << ::strerror(err) << "\n";
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
