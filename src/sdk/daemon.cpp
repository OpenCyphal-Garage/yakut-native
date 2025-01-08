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

    void start()
    {
        ipc_router_->start();

        using Ch     = ExecCmdChannel;
        auto channel = ipc_router_->makeChannel<Ch::Input, Ch::Output>("daemon");
        channel.subscribe([this](const auto& event_var) {
            //
            cetl::visit(  //
                cetl::make_overloaded(
                    [this](const Ch::Connected&) {
                        //
                        // NOLINTNEXTLINE *-vararg
                        ::syslog(LOG_DEBUG, "Ch connected.");
                        ExecCmd cmd{&memory_};
                        cmd.some_stuff.push_back('A');
                        cmd.some_stuff.push_back('Z');
                        ipc_exec_cmd_ch_->send(cmd);
                    },
                    [this](const Ch::Input& input) {
                        //
                        // NOLINTNEXTLINE *-vararg
                        ::syslog(LOG_DEBUG, "Server msg (%zu bytes).", input.some_stuff.size());
                        ipc_exec_cmd_ch_->send(input);
                    },
                    [](const Ch::Disconnected&) {
                        //
                        // NOLINTNEXTLINE *-vararg
                        ::syslog(LOG_DEBUG, "Server disconnected.");
                    }),
                event_var);
        });
        ipc_exec_cmd_ch_ = std::move(channel);
    }

private:
    using ExecCmd        = common::node_command::ExecCmd_1_0;
    using ExecCmdChannel = common::ipc::Channel<ExecCmd, ExecCmd>;

    cetl::pmr::memory_resource&    memory_;
    common::ipc::ClientRouter::Ptr ipc_router_;
    cetl::optional<ExecCmdChannel> ipc_exec_cmd_ch_;

};  // DaemonImpl

}  // namespace

std::unique_ptr<Daemon> Daemon::make(cetl::pmr::memory_resource& memory, libcyphal::IExecutor& executor)
{
    auto daemon = std::make_unique<DaemonImpl>(memory, executor);
    daemon->start();
    return std::move(daemon);
}

}  // namespace sdk
}  // namespace ocvsmd
