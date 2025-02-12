//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_PLATFORM_DEFINES_HPP_INCLUDED
#define OCVSMD_PLATFORM_DEFINES_HPP_INCLUDED

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/types.hpp>

#include <spdlog/spdlog.h>

#ifdef PLATFORM_OS_TYPE_BSD
#    include "bsd/kqueue_single_threaded_executor.hpp"
#else
#    include "linux/epoll_single_threaded_executor.hpp"
#endif

#include <algorithm>
#include <chrono>

namespace ocvsmd
{
namespace platform
{

#ifdef PLATFORM_OS_TYPE_BSD
using SingleThreadedExecutor = bsd::KqueueSingleThreadedExecutor;
#else
using SingleThreadedExecutor = Linux::EpollSingleThreadedExecutor;
#endif

/// Waits for the predicate to be fulfilled by spinning the executor and its awaitable resources.
///
template <typename Executor, typename Predicate>
void waitPollingUntil(Executor& executor, Predicate predicate)
{
    spdlog::trace("Waiting for predicate to be fulfilled...");

    libcyphal::Duration worst_lateness{0};
    while (!predicate())
    {
        const auto spin_result = executor.spinOnce();
        worst_lateness         = std::max(worst_lateness, spin_result.worst_lateness);

        // Above `spinOnce` might fulfill the predicate.
        if (predicate())
        {
            break;
        }

        // Poll awaitable resources but awake at least once per second.
        libcyphal::Duration timeout{std::chrono::seconds{1}};
        if (spin_result.next_exec_time.has_value())
        {
            timeout = std::min(timeout, spin_result.next_exec_time.value() - executor.now());
        }

        if (const auto poll_failure = executor.pollAwaitableResourcesFor(cetl::make_optional(timeout)))
        {
            (void) poll_failure;
            spdlog::warn("Failed to poll awaitable resources.");  // TODO: Log the error.
        }
    }

    spdlog::trace("Predicate is fulfilled (worst_lateness={}us).",
                  std::chrono::duration_cast<std::chrono::microseconds>(worst_lateness).count());
}

}  // namespace platform
}  // namespace ocvsmd

#endif  // OCVSMD_PLATFORM_DEFINES_HPP_INCLUDED
