//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_ENGINE_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
#define OCVSMD_DAEMON_ENGINE_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED

#include "debian/epoll_single_threaded_executor.hpp"

namespace ocvsmd
{
namespace daemon
{
namespace engine
{
namespace platform
{

using SingleThreadedExecutor = debian::EpollSingleThreadedExecutor;

}  // namespace platform
}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd

#endif  // OCVSMD_DAEMON_ENGINE_PLATFORM_SINGLE_THREADED_EXECUTOR_HPP_INCLUDED
