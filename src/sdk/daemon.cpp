//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/sdk/daemon.hpp>

#include "ipc/unix_socket_client.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/visit_helpers.hpp>
#include <libcyphal/executor.hpp>

#include <cstdint>
#include <memory>
#include <unistd.h>
#include <utility>

namespace ocvsmd
{
namespace sdk
{
namespace
{

class DaemonImpl final : public Daemon
{
public:
    DaemonImpl(cetl::pmr::memory_resource& memory, libcyphal::IExecutor& executor)
        : memory_{memory}
        , executor_{executor}
    {
    }

    bool connect()
    {
        return 0 == ipc_client_.start([](const auto& server_event) {
            //
            using ServerEvent = common::ipc::UnixSocketClient::ServerEvent;

            cetl::visit(  //
                cetl::make_overloaded(
                    [](const ServerEvent::Connected&) {
                        //
                        // NOLINTNEXTLINE *-vararg
                        ::syslog(LOG_DEBUG, "Server connected.");
                    },
                    [](const ServerEvent::Message& message) {
                        //
                        // NOLINTNEXTLINE *-vararg
                        ::syslog(LOG_DEBUG, "Server msg (%zu bytes).", message.payload.size());
                    },
                    [](const ServerEvent::Disconnected&) {
                        //
                        // NOLINTNEXTLINE *-vararg
                        ::syslog(LOG_DEBUG, "Server disconnected.");
                    }),
                server_event);
            return 0;
        });
    }

private:
    cetl::pmr::memory_resource&   memory_;
    libcyphal::IExecutor&         executor_;
    common::ipc::UnixSocketClient ipc_client_{executor_, "/var/run/ocvsmd/local.sock"};

};  // DaemonImpl

}  // namespace

std::unique_ptr<Daemon> Daemon::make(cetl::pmr::memory_resource& memory, libcyphal::IExecutor& executor)
{
    auto daemon = std::make_unique<DaemonImpl>(memory, executor);
    if (!daemon->connect())
    {
        return nullptr;
    }
    return std::move(daemon);
}

}  // namespace sdk
}  // namespace ocvsmd
