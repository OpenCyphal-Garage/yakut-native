//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "unix_socket_client.hpp"

#include "ocvsmd/platform/posix_utils.hpp"

#include <cetl/cetl.hpp>
#include <libcyphal/executor.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <utility>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

UnixSocketClient::UnixSocketClient(libcyphal::IExecutor& executor, std::string socket_path)
    : Base{executor}
    , socket_path_{std::move(socket_path)}
{
}

int UnixSocketClient::makeSocketHandle(int& out_fd)
{
    CETL_DEBUG_ASSERT(out_fd == -1, "");

    if (const auto err = platform::posixSyscallError([&out_fd] {
            //
            return out_fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        }))
    {
        logger().error("Failed to create socket: {}.", std::strerror(err));
        return err;
    }

    sockaddr_un addr{};
    addr.sun_family                        = AF_UNIX;
    const std::string abstract_socket_path = '\0' + socket_path_;
    CETL_DEBUG_ASSERT(abstract_socket_path.size() <= sizeof(addr.sun_path), "");
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    std::memcpy(addr.sun_path,
                abstract_socket_path.c_str(),
                std::min(sizeof(addr.sun_path), abstract_socket_path.size()));

    return connectSocket(out_fd, &addr, offsetof(struct sockaddr_un, sun_path) + abstract_socket_path.size());
}

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
