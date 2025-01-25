//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/sdk/daemon.hpp>

#include "ipc/channel.hpp"
#include "ipc/client_router.hpp"
#include "ipc/pipe/net_socket_client.hpp"
// #include "ipc/pipe/unix_socket_client.hpp"
#include "logging.hpp"
#include "ocvsmd/sdk/node_command_client.hpp"
#include "sdk_factory.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>

#include <cstring>
#include <memory>
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
        , logger_{common::getLogger("sdk")}
    {
        // using ClientPipe = common::ipc::pipe::UnixSocketClient;
        // auto client_pipe = std::make_unique<ClientPipe>(executor, "/var/run/ocvsmd/local.sock");
        using ClientPipe = common::ipc::pipe::NetSocketClient;
        auto client_pipe = std::make_unique<ClientPipe>(executor, "127.0.0.1", 9875);  // NOLINT(*-magic-numbers)

        ipc_router_ = common::ipc::ClientRouter::make(memory, std::move(client_pipe));

        node_command_client_ = Factory::makeNodeCommandClient(memory, ipc_router_);
    }

    CETL_NODISCARD int start() const
    {
        if (const int err = ipc_router_->start())
        {
            logger_->error("Failed to start IPC router: {}.", std::strerror(err));
            return err;
        }

        return 0;
    }

    // Daemon

    NodeCommandClient::Ptr getNodeCommandClient() override
    {
        return node_command_client_;
    }

private:
    cetl::pmr::memory_resource&    memory_;
    common::LoggerPtr              logger_;
    common::ipc::ClientRouter::Ptr ipc_router_;
    NodeCommandClient::Ptr         node_command_client_;

};  // DaemonImpl

}  // namespace

CETL_NODISCARD Daemon::Ptr Daemon::make(  //
    cetl::pmr::memory_resource& memory,
    libcyphal::IExecutor&       executor)
{
    auto daemon = std::make_shared<DaemonImpl>(memory, executor);
    if (0 != daemon->start())
    {
        return nullptr;
    }

    return daemon;
}

}  // namespace sdk
}  // namespace ocvsmd
