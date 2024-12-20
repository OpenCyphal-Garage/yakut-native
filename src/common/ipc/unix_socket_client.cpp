//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "unix_socket_client.hpp"

#include <array>
#include <cerrno>
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
        ::close(client_fd_);
    }
}

bool UnixSocketClient::connect_to_server()
{
    client_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd_ == -1)
    {
        std::cerr << "Failed to create socket: " << ::strerror(errno) << "\n";
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    ::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (::connect(client_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        std::cerr << "Failed to connect to server: " << ::strerror(errno) << "\n";
        return false;
    }

    return true;
}

void UnixSocketClient::send_message(const std::string& message) const
{
    if (::write(client_fd_, message.c_str(), message.size()) == -1)
    {
        std::cerr << "Failed to send message: " << ::strerror(errno) << "\n";
    }

    constexpr std::size_t      buf_size = 256;
    std::array<char, buf_size> buffer{};
    const ssize_t              bytes_read = ::read(client_fd_, buffer.data(), buffer.size() - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
        std::cout << "Received: " << buffer.data() << "\n";
    }
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
