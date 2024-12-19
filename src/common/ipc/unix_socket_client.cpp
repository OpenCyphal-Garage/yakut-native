//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "unix_socket_client.hpp"

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
    ::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(client_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        std::cerr << "Failed to connect to server: " << ::strerror(errno) << "\n";
        return false;
    }

    return true;
}

void UnixSocketClient::send_message(const std::string& message)
{
    if (::write(client_fd_, message.c_str(), message.size()) == -1)
    {
        std::cerr << "Failed to send message: " << ::strerror(errno) << "\n";
    }

    char    buffer[256];
    ssize_t bytes_read = ::read(client_fd_, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0)
    {
        buffer[bytes_read] = '\0';
        std::cout << "Received: " << buffer << "\n";
    }
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
