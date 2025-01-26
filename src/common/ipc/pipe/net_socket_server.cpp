//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "net_socket_server.hpp"

#include "logging.hpp"
#include "ocvsmd/platform/posix_utils.hpp"

#include <cetl/cetl.hpp>
#include <libcyphal/executor.hpp>

#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

NetSocketServer::NetSocketServer(libcyphal::IExecutor& executor, const int server_port)
    : Base{executor}
    , server_port_{server_port}
{
}

int NetSocketServer::makeSocketHandle(int& out_fd)
{
    CETL_DEBUG_ASSERT(out_fd == -1, "");

    if (const auto err = platform::posixSyscallError([&out_fd] {
            //
            return out_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        }))
    {
        logger().error("Failed to create server socket: {}.", std::strerror(err));
        return err;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(server_port_);

    return bindSocket(out_fd, &addr, sizeof(addr));
}

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
