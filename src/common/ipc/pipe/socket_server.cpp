//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "socket_server.hpp"

#include "client_context.hpp"
#include "ipc/ipc_types.hpp"
#include "logging.hpp"
#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "ocvsmd/platform/posix_utils.hpp"
#include "socket_base.hpp"

#include <cetl/cetl.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>

#include <cerrno>
#include <cstring>
#include <memory>
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
namespace
{

constexpr int MaxConnections = 5;

}  // namespace

SocketServer::SocketServer(libcyphal::IExecutor& executor, const io::SocketAddress& address)
    : socket_address_{address}
    , posix_executor_ext_{cetl::rtti_cast<platform::IPosixExecutorExtension*>(&executor)}
    , unique_client_id_counter_{0}
{
    CETL_DEBUG_ASSERT(posix_executor_ext_ != nullptr, "");
}

int SocketServer::start(EventHandler event_handler)
{
    CETL_DEBUG_ASSERT(event_handler, "");
    CETL_DEBUG_ASSERT(static_cast<int>(server_fd_) == -1, "");

    event_handler_ = std::move(event_handler);

    if (const auto err = makeSocketHandle())
    {
        logger().error("Failed to make socket handle: {}.", std::strerror(err));
        return err;
    }

    if (const auto err = platform::posixSyscallError([this] {
            //
            return ::listen(static_cast<int>(server_fd_), MaxConnections);
        }))
    {
        logger().error("Failed to listen on server socket: {}.", std::strerror(err));
        return err;
    }

    accept_callback_ = posix_executor_ext_->registerAwaitableCallback(  //
        [this](const auto&) {
            //
            handleAccept();
        },
        platform::IPosixExecutorExtension::Trigger::Readable{static_cast<int>(server_fd_)});

    return 0;
}

int SocketServer::makeSocketHandle()
{
    using SocketResult = io::SocketAddress::SocketResult;

    auto maybe_socket = socket_address_.socket(SOCK_STREAM);
    if (auto* const err = cetl::get_if<SocketResult::Failure>(&maybe_socket))
    {
        logger().error("Failed to create server socket: {}.", std::strerror(*err));
        return *err;
    }
    auto socket_fd = cetl::get<SocketResult::Success>(std::move(maybe_socket));
    CETL_DEBUG_ASSERT(static_cast<int>(socket_fd) != -1, "");

    const int err = socket_address_.bind(socket_fd);
    if (err != 0)
    {
        logger().error("Failed to bind server socket: {}.", std::strerror(err));
        return err;
    }

    server_fd_ = std::move(socket_fd);
    return 0;
}

int SocketServer::send(const ClientId client_id, const Payloads payloads)
{
    if (auto* const client_context = tryFindClientContext(client_id))
    {
        return SocketBase::send(client_context->state(), payloads);
    }

    logger().warn("Client context is not found (id={}).", client_id);
    return EINVAL;
}

void SocketServer::handleAccept()
{
    CETL_DEBUG_ASSERT(static_cast<int>(server_fd_) != -1, "");

    io::OwnFd client_fd;
    if (const auto err = platform::posixSyscallError([this, &client_fd] {
            //
            client_fd = io::OwnFd{::accept(static_cast<int>(server_fd_), nullptr, nullptr)};
            return static_cast<int>(client_fd);
        }))
    {
        logger().warn("Failed to accept client connection: {}.", std::strerror(err));
        return;
    }
    const int raw_fd = static_cast<int>(client_fd);
    CETL_DEBUG_ASSERT(raw_fd != -1, "");

    const ClientId new_client_id  = ++unique_client_id_counter_;
    auto           client_context = std::make_unique<ClientContext>(new_client_id, std::move(client_fd), logger());
    //
    client_context->setCallback(posix_executor_ext_->registerAwaitableCallback(
        [this, new_client_id](const auto&) {
            //
            handleClientRequest(new_client_id);
        },
        platform::IPosixExecutorExtension::Trigger::Readable{raw_fd}));

    client_id_to_context_.emplace(new_client_id, std::move(client_context));

    event_handler_(Event::Connected{new_client_id});
}

void SocketServer::handleClientRequest(const ClientId client_id)
{
    auto* const client_context = tryFindClientContext(client_id);
    CETL_DEBUG_ASSERT(client_context, "");
    auto& state = client_context->state();

    if (const auto err = receiveMessage(state, [this, client_id](const auto payload) {
            //
            return event_handler_(Event::Message{client_id, payload});
        }))
    {
        if (err == -1)
        {
            logger().debug("End of client stream - closing connection (id={}, fd={}).",
                           client_id,
                           static_cast<int>(state.fd));
        }
        else
        {
            logger().warn("Failed to handle client request - closing connection (id={}, fd={}): {}.",
                          client_id,
                          static_cast<int>(state.fd),
                          std::strerror(err));
        }

        client_id_to_context_.erase(client_id);
        event_handler_(Event::Disconnected{client_id});
    }
}

ClientContext* SocketServer::tryFindClientContext(const ClientId client_id)
{
    const auto id_and_context = client_id_to_context_.find(client_id);
    if (id_and_context != client_id_to_context_.end())
    {
        return id_and_context->second.get();
    }
    return nullptr;
}

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
