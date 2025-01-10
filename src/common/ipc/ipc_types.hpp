//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_TYPES_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_TYPES_HPP_INCLUDED

#include <cetl/pf20/cetlpf.hpp>

#include <cerrno>
#include <cstdint>
#include <sys/syslog.h>

namespace ocvsmd
{
namespace common
{
namespace ipc
{

/// Defines some common error codes of IPC operations.
///
/// Maps to `errno` values, hence `int` inheritance and zero on success.
///
enum class ErrorCode : int  // NOLINT
{
    Success      = 0,
    NotConnected = ENOTCONN,
    Disconnected = ESHUTDOWN,

};  // ErrorCode

using Payload  = cetl::span<const std::uint8_t>;
using Payloads = cetl::span<const Payload>;

template <typename Action>
void performWithoutThrowing(Action&& action) noexcept
{
#if defined(__cpp_exceptions)
    try
#endif
    {
        std::forward<Action>(action)();
    }
#if defined(__cpp_exceptions)
    catch (...)
    {
        ::syslog(LOG_WARNING, "Unexpected exception is caught!");  // NOLINT
    }
#endif
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_TYPES_HPP_INCLUDED
