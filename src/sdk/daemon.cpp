//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/sdk/daemon.hpp>

#include "ipc/channel.hpp"
#include "ipc/client_router.hpp"
#include "ipc/pipe/client_pipe.hpp"
#include "ipc/pipe/unix_socket_client.hpp"

#include "ocvsmd/common/node_command/ExecCmd_1_0.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/visit_helpers.hpp>
#include <libcyphal/executor.hpp>

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
    {
        using ClientPipe = common::ipc::pipe::UnixSocketClient;

        auto client_pipe = std::make_unique<ClientPipe>(executor, "/var/run/ocvsmd/local.sock");
        ipc_router_      = common::ipc::ClientRouter::make(memory, std::move(client_pipe));
    }

    CETL_NODISCARD int start()
    {
        const int result = ipc_router_->start();
        if (result != 0)
        {
            // NOLINTNEXTLINE *-vararg
            ::syslog(LOG_ERR, "Failed to start IPC router: %d", result);
            return result;
        }

        auto channel = ipc_router_->makeChannel<ExecCmdChannel>("daemon");
        channel.subscribe([this](const auto& event_var) {
            //
            cetl::visit(  //
                cetl::make_overloaded(
                    [this](const ExecCmdChannel::Connected&) {
                        //
                        // NOLINTNEXTLINE *-vararg
                        ::syslog(LOG_DEBUG, "🟢 Ch connected.");
                        ExecCmd cmd{&memory_};
                        cmd.some_stuff.push_back('A');
                        cmd.some_stuff.push_back('Z');
                        const int result = ipc_exec_cmd_ch_->send(cmd);
                        (void) result;
                    },
                    [this](const ExecCmdChannel::Input& input) {
                        //
                        // NOLINTNEXTLINE *-vararg
                        ::syslog(LOG_DEBUG, "🔵 Ch Msg (%zu bytes).", input.some_stuff.size());
                        const int result = ipc_exec_cmd_ch_->send(input);
                        (void) result;
                    },
                    [](const ExecCmdChannel::Completed& completed) {
                        //
                        // NOLINTNEXTLINE *-vararg
                        ::syslog(LOG_DEBUG, "🔴 Ch Completed (err=%d).", static_cast<int>(completed.error_code));
                    }),
                event_var);
        });
        ipc_exec_cmd_ch_ = std::move(channel);

        return 0;
    }

private:
    using ExecCmd        = common::node_command::ExecCmd_1_0;
    using ExecCmdChannel = common::ipc::Channel<ExecCmd, ExecCmd>;

    cetl::pmr::memory_resource&    memory_;
    common::ipc::ClientRouter::Ptr ipc_router_;
    cetl::optional<ExecCmdChannel> ipc_exec_cmd_ch_;

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
