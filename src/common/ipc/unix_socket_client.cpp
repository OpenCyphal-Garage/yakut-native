//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "unix_socket_client.hpp"

#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "ocvsmd/platform/posix_utils.hpp"

#include <cetl/cetl.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
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

UnixSocketClient::UnixSocketClient(libcyphal::IExecutor& executor, std::string socket_path)
    : socket_path_{std::move(socket_path)}
    , client_fd_{-1}
    , posix_executor_ext_{cetl::rtti_cast<platform::IPosixExecutorExtension*>(&executor)}
{
    CETL_DEBUG_ASSERT(posix_executor_ext_ != nullptr, "");
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

int UnixSocketClient::start(std::function<int(const ServerEvent::Var&)>&& server_event_handler)
{
    CETL_DEBUG_ASSERT(client_fd_ == -1, "");
    CETL_DEBUG_ASSERT(server_event_handler, "");

    server_event_handler_ = std::move(server_event_handler);

    if (const auto err = platform::posixSyscallError([this] {
            //
            return client_fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        }))
    {
        std::cerr << "Failed to create socket: " << std::strerror(err) << "\n";
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

    if (const auto err = platform::posixSyscallError([this, &addr, &abstract_socket_path] {
            //
            return ::connect(client_fd_,
                             // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                             reinterpret_cast<const sockaddr*>(&addr),
                             offsetof(struct sockaddr_un, sun_path) + abstract_socket_path.size());
        }))
    {
        std::cerr << "Failed to connect to server: " << std::strerror(err) << "\n";
        return err;
    }

    socket_callback_ = posix_executor_ext_->registerAwaitableCallback(  //
        [this](const auto&) {
            //
            handle_socket();
        },
        platform::IPosixExecutorExtension::Trigger::Readable{client_fd_});

    server_event_handler_(ServerEvent::Connected{});
    return 0;
}

void UnixSocketClient::handle_socket()
{
    if (const auto err = receiveMessage(client_fd_, [this](const auto payload) {
            //
            return server_event_handler_(ServerEvent::Message{payload});
        }))
    {
        if (err == -1)
        {
            ::syslog(LOG_DEBUG, "End of server stream - closing connection.");
        }
        else
        {
            ::syslog(LOG_WARNING, "Failed to handle server response - closing connection: %s", std::strerror(err));
        }

        socket_callback_.reset();
        ::close(client_fd_);
        client_fd_ = -1;

        server_event_handler_(ServerEvent::Disconnected{});
    }
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
