//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_ENGINE_SVC_HELPERS_HPP_INCLUDED
#define OCVSMD_DAEMON_ENGINE_SVC_HELPERS_HPP_INCLUDED

#include "ipc/server_router.hpp"

#include <nunavut/support/serialization.hpp>

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/errors.hpp>
#include <libcyphal/executor.hpp>
#include <libcyphal/presentation/client.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/response_promise.hpp>
#include <libcyphal/transport/errors.hpp>

#include <cerrno>

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

inline int errorToCode(const libcyphal::MemoryError) noexcept
{
    return ENOMEM;
}
inline int errorToCode(const libcyphal::transport::CapacityError) noexcept
{
    return ENOMEM;
}

inline int errorToCode(const libcyphal::ArgumentError) noexcept
{
    return EINVAL;
}
inline int errorToCode(const libcyphal::transport::AnonymousError) noexcept
{
    return EINVAL;
}
inline int errorToCode(const nunavut::support::Error) noexcept
{
    return EINVAL;
}

inline int errorToCode(const libcyphal::transport::AlreadyExistsError) noexcept
{
    return EEXIST;
}

inline int errorToCode(const libcyphal::transport::PlatformError& platform_error) noexcept
{
    return static_cast<int>(platform_error->code());
}

inline int errorToCode(const libcyphal::presentation::ResponsePromiseExpired) noexcept
{
    return ETIMEDOUT;
}

inline int errorToCode(const libcyphal::presentation::detail::ClientBase::TooManyPendingRequestsError) noexcept
{
    return EBUSY;
}

template <typename Variant>
int failureToErrorCode(const Variant& failure)
{
    return cetl::visit([](const auto& error) { return errorToCode(error); }, failure);
}

}  // namespace svc
}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd

#endif  // OCVSMD_DAEMON_ENGINE_SVC_HELPERS_HPP_INCLUDED
