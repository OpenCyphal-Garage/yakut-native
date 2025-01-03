//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/platform/defines.hpp>
#include <ocvsmd/sdk/daemon.hpp>

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/types.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <signal.h>  // NOLINT
#include <sys/syslog.h>

namespace
{

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile sig_atomic_t g_running = 1;

void signal_handler(const int sig)
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

void setup_signal_handlers()
{
    struct sigaction sigbreak
    {};
    sigbreak.sa_handler = &signal_handler;
    ::sigaction(SIGINT, &sigbreak, nullptr);
    ::sigaction(SIGTERM, &sigbreak, nullptr);
}

}  // namespace

int main(const int, const char** const)
{
    using std::chrono_literals::operator""s;

    setup_signal_handlers();

    ::openlog("ocvsmd-cli", LOG_PID, LOG_USER);
    ::syslog(LOG_NOTICE, "ocvsmd cli started.");  // NOLINT *-vararg
    {
        auto&                                    memory = *cetl::pmr::new_delete_resource();
        ocvsmd::platform::SingleThreadedExecutor executor;

        const auto daemon = ocvsmd::sdk::Daemon::make(memory, executor);
        if (!daemon)
        {
            std::cerr << "Failed to create daemon.\n";
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

            // TODO: Don't ignore polling failures; come up with a strategy to handle them.
            // Probably we should log it, break the loop,
            // and exit with a failure code (b/c it is a critical and unexpected error).
            auto maybe_poll_failure = executor.pollAwaitableResourcesFor(cetl::make_optional(timeout));
            (void) maybe_poll_failure;
        }

        if (g_running == 0)
        {
            ::syslog(LOG_NOTICE, "Received termination signal.");  // NOLINT *-vararg
        }
    }
    ::syslog(LOG_NOTICE, "ocvsmd cli terminated.");  // NOLINT *-vararg
    ::closelog();

    return 0;
}
