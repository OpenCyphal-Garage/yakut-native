//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_PLATFORM_DEFINES_HPP_INCLUDED
#define OCVSMD_PLATFORM_DEFINES_HPP_INCLUDED

#ifdef PLATFORM_OS_TYPE_BSD
#    include "bsd/kqueue_single_threaded_executor.hpp"
#else
#    include "linux/epoll_single_threaded_executor.hpp"
#endif

namespace ocvsmd
{
namespace platform
{

#ifdef PLATFORM_OS_TYPE_BSD
using SingleThreadedExecutor = bsd::KqueueSingleThreadedExecutor;
#else
using SingleThreadedExecutor = Linux::EpollSingleThreadedExecutor;
#endif

}  // namespace platform
}  // namespace ocvsmd

#endif  // OCVSMD_PLATFORM_DEFINES_HPP_INCLUDED
