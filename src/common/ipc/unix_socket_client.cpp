//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "unix_socket_client.hpp"

#include "platform/posix_utils.hpp"

#include <cetl/cetl.hpp>

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

UnixSocketClient::UnixSocketClient(std::string socket_path)
    : socket_path_{std::move(socket_path)}
    , client_fd_{-1}
{
}

UnixSocketClient::~UnixSocketClient()
{
    if (client_fd_ != -1)
    {
        platform::posixSyscallError([this] {
            //
            return ::close(client_fd_);
        });
    }
}

bool UnixSocketClient::connect_to_server()
{
    CETL_DEBUG_ASSERT(client_fd_ == -1, "");

    if (const auto err = platform::posixSyscallError([this] {
            //
            return client_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        }))
    {
        std::cerr << "Failed to create socket: " << ::strerror(err) << "\n";
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    ::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (const auto err = platform::posixSyscallError([this, &addr] {
            //
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return ::connect(client_fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        }))
    {
        std::cerr << "Failed to connect to server: " << ::strerror(err) << "\n";
        return false;
    }

    return true;
}

void UnixSocketClient::send_message(const std::string& message) const
{
    if (const auto err = platform::posixSyscallError([this, &message] {
            //
            return ::write(client_fd_, message.c_str(), message.size());
        }))
    {
        std::cerr << "Failed to write: " << ::strerror(err) << "\n";
        return;
    }

    constexpr std::size_t      buf_size = 256;
    std::array<char, buf_size> buffer{};
    ssize_t                    bytes_read = 0;
    if (const auto err = platform::posixSyscallError([this, &bytes_read, &buffer] {
            //
            return bytes_read = ::read(client_fd_, buffer.data(), buffer.size() - 1);
        }))
    {
        std::cerr << "Failed to read: " << ::strerror(err) << "\n";
        return;
    }
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        std::cout << "Received: " << buffer.data() << "\n";
    }
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
