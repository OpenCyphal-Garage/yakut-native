//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_HELPERS_HPP_INCLUDED
#define OCVSMD_COMMON_HELPERS_HPP_INCLUDED

#include <spdlog/spdlog.h>

#include <exception>
#include <utility>

namespace ocvsmd
{
namespace common
{

/// @brief Wraps the given action into a try/catch block, and performs it without throwing the given exception type.
///
/// @return `true` if the action was performed successfully, `false` if an exception was thrown.
///         Always `true` if exceptions are disabled.
///
template <typename Exception = std::exception, typename Action>
bool performWithoutThrowing(Action&& action) noexcept
{
#if defined(__cpp_exceptions)
    try
    {
#endif
        std::forward<Action>(action)();
        return true;

#if defined(__cpp_exceptions)
    } catch (const Exception& ex)
    {
        spdlog::critical("Unexpected C++ exception is caught: {}", ex.what());
        return false;
    }
#endif
}

}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_HELPERS_HPP_INCLUDED
