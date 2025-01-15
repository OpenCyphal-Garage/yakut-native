//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "unix_socket_server.hpp"

#include "logging.hpp"
#include "ocvsmd/platform/posix_executor_extension.hpp"
#include "ocvsmd/platform/posix_utils.hpp"

#include <cetl/cetl.hpp>
#include <cetl/rtti.hpp>
#include <libcyphal/executor.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
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
namespace
{

constexpr int MaxConnections = 5;

class ClientContextImpl final : public detail::ClientContext
{
public:
    ClientContextImpl(const UnixSocketServer::ClientId id, const int fd, Logger& logger)
        : id_{id}
        , fd_{fd}
        , logger_{logger}
    {
        CETL_DEBUG_ASSERT(fd_ != -1, "");

        logger_.trace("ClientContextImpl(fd={}, id={}).", fd_, id_);
    }

    ~ClientContextImpl() override
    {
        logger_.trace("~ClientContextImpl(fd={}, id={}).", fd_, id_);

        platform::posixSyscallError([this] {
            //
            return ::close(fd_);
        });
    }

    ClientContextImpl(const ClientContextImpl&)                = delete;
    ClientContextImpl(ClientContextImpl&&) noexcept            = delete;
    ClientContextImpl& operator=(const ClientContextImpl&)     = delete;
    ClientContextImpl& operator=(ClientContextImpl&&) noexcept = delete;

    void setCallback(libcyphal::IExecutor::Callback::Any&& fd_callback)
    {
        fd_callback_ = std::move(fd_callback);
    }

private:
    const UnixSocketServer::ClientId    id_;
    const int                           fd_;
    Logger&                             logger_;
    libcyphal::IExecutor::Callback::Any fd_callback_;

};  // ClientContextImpl

}  // namespace

UnixSocketServer::UnixSocketServer(libcyphal::IExecutor& executor, std::string socket_path)
    : socket_path_{std::move(socket_path)}
    , server_fd_{-1}
    , posix_executor_ext_{cetl::rtti_cast<platform::IPosixExecutorExtension*>(&executor)}
    , unique_client_id_counter_{0}
{
    CETL_DEBUG_ASSERT(posix_executor_ext_ != nullptr, "");
}

UnixSocketServer::~UnixSocketServer()
{
    if (server_fd_ != -1)
    {
        platform::posixSyscallError([this] {
            //
            return ::close(server_fd_);
        });
    }
}

CETL_NODISCARD int UnixSocketServer::start(EventHandler event_handler)
{
    CETL_DEBUG_ASSERT(server_fd_ == -1, "");
    CETL_DEBUG_ASSERT(event_handler, "");

    event_handler_ = std::move(event_handler);

    if (const auto err = platform::posixSyscallError([this] {
            //
            return server_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        }))
    {
        logger().error("Failed to create server socket: {}.", std::strerror(err));
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
            return ::bind(server_fd_,
                          // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                          reinterpret_cast<const sockaddr*>(&addr),
                          offsetof(struct sockaddr_un, sun_path) + abstract_socket_path.size());
        }))
    {
        logger().error("Failed to bind server socket: {}.", std::strerror(err));
        return err;
    }

    if (const auto err = platform::posixSyscallError([this] {
            //
            return ::listen(server_fd_, MaxConnections);
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
        platform::IPosixExecutorExtension::Trigger::Readable{server_fd_});

    return 0;
}

void UnixSocketServer::handleAccept()
{
    CETL_DEBUG_ASSERT(server_fd_ != -1, "");

    int client_fd = -1;
    if (const auto err = platform::posixSyscallError([this, &client_fd] {
            //
            return client_fd = ::accept(server_fd_, nullptr, nullptr);
        }))
    {
        logger().warn("Failed to accept client connection: {}.", std::strerror(err));
        return;
    }

    CETL_DEBUG_ASSERT(client_fd != -1, "");
    CETL_DEBUG_ASSERT(client_fd_to_context_.find(client_fd) == client_fd_to_context_.end(), "");

    const ClientId new_client_id  = ++unique_client_id_counter_;
    auto           client_context = std::make_unique<ClientContextImpl>(new_client_id, client_fd, logger());
    //
    client_context->setCallback(posix_executor_ext_->registerAwaitableCallback(
        [this, new_client_id, client_fd](const auto&) {
            //
            handleClientRequest(new_client_id, client_fd);
        },
        platform::IPosixExecutorExtension::Trigger::Readable{client_fd}));

    client_id_to_fd_[new_client_id] = client_fd;
    client_fd_to_context_.emplace(client_fd, std::move(client_context));

    event_handler_(Event::Connected{new_client_id});
}

void UnixSocketServer::handleClientRequest(const ClientId client_id, const int client_fd)
{
    if (const auto err = receiveMessage(client_fd, [this, client_id](const auto payload) {
            //
            return event_handler_(Event::Message{client_id, payload});
        }))
    {
        if (err == -1)
        {
            logger().debug("End of client stream - closing connection (id={}, fd={}).", client_id, client_fd);
        }
        else
        {
            logger().warn("Failed to handle client request - closing connection (id={}, fd={}): {}.",
                          client_id,
                          client_fd,
                          std::strerror(err));
        }

        client_id_to_fd_.erase(client_id);
        client_fd_to_context_.erase(client_fd);

        event_handler_(Event::Disconnected{client_id});
    }
}

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
