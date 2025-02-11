//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_ENGINE_SVC_HELPERS_HPP_INCLUDED
#define OCVSMD_DAEMON_ENGINE_SVC_HELPERS_HPP_INCLUDED

#include "ipc/server_router.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/presentation/presentation.hpp>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{
namespace svc
{

struct ScvContext
{
    cetl::pmr::memory_resource&            memory;
    libcyphal::IExecutor&                  executor;
    common::ipc::ServerRouter&             ipc_router;
    libcyphal::presentation::Presentation& presentation;

};  // ScvContext

}  // namespace svc
}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd

#endif  // OCVSMD_DAEMON_ENGINE_SVC_HELPERS_HPP_INCLUDED
