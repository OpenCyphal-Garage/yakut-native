//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/sdk/daemon.hpp>

#include "ipc/channel.hpp"
#include "ipc/client_router.hpp"
#include "ipc/pipe/unix_socket_client.hpp"
#include "logging.hpp"

#include "ocvsmd/common/node_command/ExecCmd_0_1.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/visit_helpers.hpp>
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
        using ClientPipe = common::ipc::pipe::UnixSocketClient;

        auto client_pipe = std::make_unique<ClientPipe>(executor, "/var/run/ocvsmd/local.sock");
        ipc_router_      = common::ipc::ClientRouter::make(memory, std::move(client_pipe));
    }

    CETL_NODISCARD int start()
    {
        if (const int err = ipc_router_->start())
        {
            logger_->error("Failed to start IPC router: {}.", std::strerror(err));
            return err;
        }

        ipc_exec_cmd_ch_ = ipc_router_->makeChannel<ExecCmdChannel>("daemon");
        logger_->debug("C << 🆕 Ch created.");
        ipc_exec_cmd_ch_->subscribe([this](const auto& event_var) {
            //
            cetl::visit(  //
                cetl::make_overloaded(
                    [this](const ExecCmdChannel::Connected&) {
                        //
                        logger_->info("C << 🟢 Ch connected.");

                        ExecCmd cmd{&memory_};
                        cmd.some_stuff.push_back('C');
                        cmd.some_stuff.push_back('L');
                        cmd.some_stuff.push_back('\0');
                        logger_->info("C >> 🔵 Ch 'CL' msg.");
                        const int result = ipc_exec_cmd_ch_->send(cmd);
                        (void) result;
                    },
                    [this](const ExecCmdChannel::Input& input) {
                        //
                        logger_->info("C << 🔵 Ch Msg='{}'.", reinterpret_cast<const char*>(input.some_stuff.data()));

                        if (countdown_--)
                        {
                            logger_->info("C >> 🔵 Ch '{}' msg.",
                                          reinterpret_cast<const char*>(input.some_stuff.data()));
                            const int result = ipc_exec_cmd_ch_->send(input);
                            (void) result;
                        }
                    },
                    [this](const ExecCmdChannel::Completed& completed) {
                        //
                        logger_->info("C << 🔴 Ch Completed (err={}).", static_cast<int>(completed.error_code));
                        ipc_exec_cmd_ch_.reset();
                    }),
                event_var);
        });

        return 0;
    }

private:
    using ExecCmd        = common::node_command::ExecCmd_0_1;
    using ExecCmdChannel = common::ipc::Channel<ExecCmd, ExecCmd>;

    cetl::pmr::memory_resource&    memory_;
    common::LoggerPtr              logger_;
    common::ipc::ClientRouter::Ptr ipc_router_;
    cetl::optional<ExecCmdChannel> ipc_exec_cmd_ch_;

    int countdown_{2};

};  // DaemonImpl

}  // namespace

CETL_NODISCARD std::unique_ptr<Daemon> Daemon::make(  //
    cetl::pmr::memory_resource& memory,
    libcyphal::IExecutor&       executor)
{
    auto daemon = std::make_unique<DaemonImpl>(memory, executor);
    if (0 != daemon->start())
    {
        return nullptr;
    }
    return daemon;
}

}  // namespace sdk
}  // namespace ocvsmd
