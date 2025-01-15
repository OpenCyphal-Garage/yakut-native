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

template <typename Action>
bool performWithoutThrowing(Action&& action) noexcept
{
#if defined(__cpp_exceptions)
    try
#endif
    {
        std::forward<Action>(action)();
        return true;
    }
#if defined(__cpp_exceptions)
    catch (const std::exception& ex)
    {
        spdlog::critical("Unexpected C++ exception is caught: {}", ex.what());

    } catch (...)
    {
        spdlog::critical("Unexpected unknown exception is caught!");
    }
    return false;
#endif
}

}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_HELPERS_HPP_INCLUDED
