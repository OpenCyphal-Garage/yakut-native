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
namespace pipe
{

UnixSocketClient::UnixSocketClient(libcyphal::IExecutor& executor, std::string socket_path)
    : socket_path_{std::move(socket_path)}
    , posix_executor_ext_{cetl::rtti_cast<platform::IPosixExecutorExtension*>(&executor)}
{
    CETL_DEBUG_ASSERT(posix_executor_ext_ != nullptr, "");
}

UnixSocketClient::~UnixSocketClient()
{
    if (state_.fd != -1)
    {
        platform::posixSyscallError([this] {
            //
            return ::close(state_.fd);
        });
    }
}

int UnixSocketClient::start(EventHandler event_handler)
{
    CETL_DEBUG_ASSERT(event_handler, "");
    CETL_DEBUG_ASSERT(state_.fd == -1, "");

    event_handler_ = std::move(event_handler);

    if (const auto err = platform::posixSyscallError([this] {
            //
            return state_.fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
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

    if (const auto err = platform::posixSyscallError([this, &addr, &abstract_socket_path] {
            //
            return ::connect(state_.fd,
                             // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                             reinterpret_cast<const sockaddr*>(&addr),
                             offsetof(struct sockaddr_un, sun_path) + abstract_socket_path.size());
        }))
    {
        logger().error("Failed to connect to server: {}.", std::strerror(err));
        return err;
    }

    socket_callback_ = posix_executor_ext_->registerAwaitableCallback(  //
        [this](const auto&) {
            //
            handle_socket();
        },
        platform::IPosixExecutorExtension::Trigger::Readable{state_.fd});

    event_handler_(Event::Connected{});
    return 0;
}

void UnixSocketClient::handle_socket()
{
    if (const auto err = receiveMessage(state_, [this](const auto payload) {
            //
            return event_handler_(Event::Message{payload});
        }))
    {
        if (err == -1)
        {
            logger().debug("End of server stream - closing connection.");
        }
        else
        {
            logger().warn("Failed to handle server response - closing connection: {}.", std::strerror(err));
        }

        socket_callback_.reset();
        ::close(state_.fd);
        state_.fd = -1;

        event_handler_(Event::Disconnected{});
    }
}

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
