//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_PLATFORM_POSIX_UTILS_HPP_INCLUDED
#define OCVSMD_PLATFORM_POSIX_UTILS_HPP_INCLUDED

#include <cerrno>

namespace ocvsmd
{
namespace platform
{

/// Wraps a POSIX syscall and retries it if it was interrupted by a signal.
///
template <typename Call>
int posixSyscallError(const Call& call)
{
    while (call() < 0)
    {
        const int error_num = errno;
        if (error_num != EINTR)
        {
            return error_num;
        }
    }
    return 0;
}

}  // namespace platform
}  // namespace ocvsmd

#endif  // OCVSMD_PLATFORM_POSIX_UTILS_HPP_INCLUDED
