//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "net_socket_client.hpp"

#include "ocvsmd/platform/posix_utils.hpp"

#include <cetl/cetl.hpp>
#include <libcyphal/executor.hpp>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <utility>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

NetSocketClient::NetSocketClient(libcyphal::IExecutor& executor, std::string server_ip, const int server_port)
    : Base{executor}
    , server_ip_{std::move(server_ip)}
    , server_port_{server_port}
{
}

int NetSocketClient::makeSocketHandle(int& out_fd)
{
    CETL_DEBUG_ASSERT(out_fd == -1, "");

    if (const auto err = platform::posixSyscallError([&out_fd] {
            //
            return out_fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        }))
    {
        logger().error("Failed to create socket: {}.", std::strerror(err));
        return err;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(server_port_);
    if (inet_pton(AF_INET, server_ip_.c_str(), &addr.sin_addr) <= 0)
    {
        logger().error("Invalid server IP address: {}.", server_ip_);
        return EINVAL;
    }

    return connectSocket(out_fd, &addr, sizeof(addr));
}

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
