//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/platform/defines.hpp>
#include <ocvsmd/sdk/daemon.hpp>

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/types.hpp>

#include <spdlog/cfg/argv.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <signal.h>  // NOLINT
#include <string>

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

/// Sets up the logging system.
///
/// File sink is used for all loggers (with Info default level).
///
void setupLogging(const int argc, const char** const argv)
{
    using spdlog::sinks::rotating_file_sink_st;

    try
    {
        constexpr std::size_t log_max_files     = 4;
        constexpr std::size_t log_file_max_size = 16UL * 1048576UL;  // 16 MB

        const std::string log_prefix    = "ocvsmd-cli";
        const std::string log_file_nm   = log_prefix + ".log";
        const auto        log_file_path = "./" + log_file_nm;

        // Drop all existing loggers, including the default one, so that we can reconfigure them.
        spdlog::drop_all();

        const auto file_sink = std::make_shared<rotating_file_sink_st>(  //
            log_file_path,
            log_file_max_size,
            log_max_files);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P] [%n] [%l] %v");

        const auto default_logger = std::make_shared<spdlog::logger>("", file_sink);
        register_logger(default_logger);
        set_default_logger(default_logger);

        // Register specific subsystem loggers.
        //
        register_logger(std::make_shared<spdlog::logger>("ipc", file_sink));
        register_logger(std::make_shared<spdlog::logger>("sdk", file_sink));

        // Accept `SPDLOG_LEVEL` argument (like `SPDLOG_LEVEL=debug,ipc=trace`).
        spdlog::cfg::load_argv_levels(argc, argv);

    } catch (const std::exception& ex)
    {
        std::cerr << "Failed to setup logging: " << ex.what() << '\n';
        std::exit(EXIT_FAILURE);
    }
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

        while (g_running != 0)
        {
            const auto spin_result = executor.spinOnce();

            // Poll awaitable resources but awake at least once per second.
            libcyphal::Duration timeout{1s};
            if (spin_result.next_exec_time.has_value())
            {
                timeout = std::min(timeout, spin_result.next_exec_time.value() - executor.now());
            }

            if (const auto maybe_poll_failure = executor.pollAwaitableResourcesFor(cetl::make_optional(timeout)))
            {
                spdlog::warn("Failed to poll awaitable resources.");
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
