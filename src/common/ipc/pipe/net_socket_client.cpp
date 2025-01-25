//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "net_socket_client.hpp"

#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "ocvsmd/platform/posix_utils.hpp"

#include <cetl/cetl.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <string>
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

NetSocketClient::NetSocketClient(libcyphal::IExecutor& executor, std::string server_ip, const int server_port)
    : server_ip_{std::move(server_ip)}
    , server_port_{server_port}
    , posix_executor_ext_{cetl::rtti_cast<platform::IPosixExecutorExtension*>(&executor)}
{
    CETL_DEBUG_ASSERT(posix_executor_ext_ != nullptr, "");
}

NetSocketClient::~NetSocketClient()
{
    if (state_.fd != -1)
    {
        platform::posixSyscallError([this] {
            //
            return ::close(state_.fd);
        });
    }
}

int NetSocketClient::start(EventHandler event_handler)
{
    CETL_DEBUG_ASSERT(event_handler, "");
    CETL_DEBUG_ASSERT(state_.fd == -1, "");

    event_handler_ = std::move(event_handler);

    if (const auto err = platform::posixSyscallError([this] {
            //
            return state_.fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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

    if (const auto err = platform::posixSyscallError([this, &addr] {
            //
            return ::connect(state_.fd,
                             // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                             reinterpret_cast<const sockaddr*>(&addr),
                             sizeof(addr));
        }))
    {
        if (err != EINPROGRESS)
        {
            logger().error("Failed to connect to server: {}.", std::strerror(err));
            return err;
        }
    }

    socket_callback_ = posix_executor_ext_->registerAwaitableCallback(  //
        [this](const auto&) {
            //
            handle_connect();
        },
        platform::IPosixExecutorExtension::Trigger::Writable{state_.fd});

    return 0;
}

void NetSocketClient::handle_connect()
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

void NetSocketClient::handle_receive()
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

void NetSocketClient::handle_disconnect()
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
