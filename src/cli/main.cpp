//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "setup_logging.hpp"

#include <ocvsmd/platform/defines.hpp>
#include <ocvsmd/sdk/daemon.hpp>
#include <ocvsmd/sdk/execution.hpp>
#include <ocvsmd/sdk/node_command_client.hpp>

#include <cetl/pf17/cetlpf.hpp>

#include <spdlog/spdlog.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <signal.h>  // NOLINT
#include <utility>
#include <vector>

namespace
{

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile sig_atomic_t g_running = 1;

void signalHandler(const int sig)
{
    switch (sig)
    {
    case SIGINT:
    case SIGTERM:
        g_running = 0;
        break;
    default:
        break;
    }
}

void setupSignalHandlers()
{
    struct sigaction sigbreak
    {};
    sigbreak.sa_handler = &signalHandler;
    ::sigaction(SIGINT, &sigbreak, nullptr);
    ::sigaction(SIGTERM, &sigbreak, nullptr);
}

}  // namespace

int main(const int argc, const char** const argv)
{
    using std::chrono_literals::operator""s;
    using Executor = ocvsmd::platform::SingleThreadedExecutor;

    setupSignalHandlers();
    setupLogging(argc, argv);

    spdlog::info("OCVSMD client started (ver='{}.{}').", VERSION_MAJOR, VERSION_MINOR);
    int result = EXIT_SUCCESS;
    try
    {
        auto&    memory = *cetl::pmr::new_delete_resource();
        Executor executor;

        const auto daemon = ocvsmd::sdk::Daemon::make(memory, executor);
        if (!daemon)
        {
            spdlog::critical("Failed to create daemon.");
            std::cerr << "Failed to create daemon.";
            return EXIT_FAILURE;
        }

        // Demo of daemon's node command client, sending a command to node 42.
        {
            using Command      = ocvsmd::sdk::NodeCommandClient::Command;
            using CommandParam = Command::NodeRequest::_traits_::TypeOf::parameter;

            auto node_cmd_client = daemon->getNodeCommandClient();

            constexpr auto                   cmd_id   = Command::NodeRequest::COMMAND_IDENTIFY;
            const std::vector<std::uint16_t> node_ids = {42, 43, 44};
            const Command::NodeRequest       node_request{cmd_id, CommandParam{&memory}, &memory};

            auto sender     = node_cmd_client->sendCommand({node_ids.data(), node_ids.size()}, node_request, 1s);
            auto cmd_result = ocvsmd::sdk::sync_wait<Command::Result>(executor, std::move(sender));

            if (const auto* const err = cetl::get_if<Command::Failure>(&cmd_result))
            {
                spdlog::error("Failed to send command: {}", std::strerror(*err));
            }
            else
            {
                const auto responds = cetl::get<Command::Success>(std::move(cmd_result));
                for (const auto& node_and_respond : responds)
                {
                    spdlog::info("Node {} responded with status: {}.",
                                 node_and_respond.first,
                                 node_and_respond.second.status);
                }
            }
        }

        if (g_running == 0)
        {
            spdlog::debug("Received termination signal.");
        }

    } catch (const std::exception& ex)
    {
        spdlog::critical("Unhandled exception: {}", ex.what());
        result = EXIT_FAILURE;
    }
    spdlog::info("OCVSMD client terminated.");

    return result;
}
