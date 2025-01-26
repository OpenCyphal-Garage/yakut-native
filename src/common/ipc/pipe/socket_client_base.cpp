//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "socket_client_base.hpp"

#include "ipc/ipc_types.hpp"
#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "ocvsmd/platform/posix_utils.hpp"
#include "socket_base.hpp"

#include <cetl/cetl.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <sys/socket.h>
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

SocketClientBase::SocketClientBase(libcyphal::IExecutor& executor)
    : posix_executor_ext_{cetl::rtti_cast<platform::IPosixExecutorExtension*>(&executor)}
{
    CETL_DEBUG_ASSERT(posix_executor_ext_ != nullptr, "");
}

SocketClientBase::~SocketClientBase()
{
    if (state_.fd != -1)
    {
        platform::posixSyscallError([this] {
            //
            return ::close(state_.fd);
        });
    }
}

int SocketClientBase::start(EventHandler event_handler)
{
    CETL_DEBUG_ASSERT(event_handler, "");
    CETL_DEBUG_ASSERT(state_.fd == -1, "");

    event_handler_ = std::move(event_handler);

    if (const auto err = makeSocketHandle(state_.fd))
    {
        logger().error("Failed to make socket handle: {}.", std::strerror(err));
        return err;
    }

    socket_callback_ = posix_executor_ext_->registerAwaitableCallback(  //
        [this](const auto&) {
            //
            handle_connect();
        },
        platform::IPosixExecutorExtension::Trigger::Writable{state_.fd});

    return 0;
}

int SocketClientBase::send(const Payloads payloads)
{
    return SocketBase::send(state_, payloads);
}

int SocketClientBase::connectSocket(const int fd, const void* const addr_ptr, const std::size_t addr_size) const
{
    if (const auto err = platform::posixSyscallError([fd, addr_ptr, addr_size] {
            //
            return ::connect(fd, static_cast<const sockaddr*>(addr_ptr), addr_size);
        }))
    {
        if (err != EINPROGRESS)
        {
            logger().error("Failed to connect to server: {}.", std::strerror(err));
            return err;
        }
    }
    return 0;
}

void SocketClientBase::handle_connect()
{
    socket_callback_.reset();

    int so_error = 0;
    if (const auto err = platform::posixSyscallError([this, &so_error] {
            //
            socklen_t len = sizeof(so_error);
            return ::getsockopt(state_.fd, SOL_SOCKET, SO_ERROR, &so_error, &len);  // NOLINT(misc-include-cleaner)
        }))
    {
        logger().warn("Failed to query socket error: {}.", std::strerror(err));
        so_error = err;
    }
    if (so_error != 0)
    {
        logger().error("Failed to connect to server: {}.", std::strerror(so_error));
        handle_disconnect();
        return;
    }

    socket_callback_ = posix_executor_ext_->registerAwaitableCallback(  //
        [this](const auto&) {
            //
            handle_receive();
        },
        platform::IPosixExecutorExtension::Trigger::Readable{state_.fd});

    state_.read_phase = State::ReadPhase::Header;
    event_handler_(Event::Connected{});
}

void SocketClientBase::handle_receive()
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

        handle_disconnect();
    }
}

void SocketClientBase::handle_disconnect()
{
    socket_callback_.reset();

    ::close(state_.fd);
    state_.fd         = -1;
    state_.read_phase = State::ReadPhase::Header;

    event_handler_(Event::Disconnected{});
}

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
