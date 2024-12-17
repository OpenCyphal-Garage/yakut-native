//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "application.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/types.hpp>

#include <algorithm>
#include <functional>
#include <string>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{

cetl::optional<std::string> Application::init()
{
    (void) executor_;
    // return "not implemented";
    return cetl::nullopt;
}

void Application::runWith(const std::function<bool()>& loop_predicate)
{
    using std::chrono_literals::operator""s;

    while (loop_predicate())
    {
        const auto spin_result = executor_.spinOnce();

        // Poll awaitable resources but awake at least once per second.
        libcyphal::Duration timeout{1s};
        if (spin_result.next_exec_time.has_value())
        {
            timeout = std::min(timeout, spin_result.next_exec_time.value() - executor_.now());
        }
        (void) executor_.pollAwaitableResourcesFor(cetl::make_optional(timeout));
    }
}

}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd
