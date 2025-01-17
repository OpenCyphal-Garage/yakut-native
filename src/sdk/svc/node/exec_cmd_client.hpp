//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_SDK_SVC_NODE_EXEC_CMD_CLIENT_HPP_INCLUDED
#define OCVSMD_SDK_SVC_NODE_EXEC_CMD_CLIENT_HPP_INCLUDED

#include "ipc/client_router.hpp"
#include "svc/node/exec_cmd_spec.hpp"

#include <uavcan/node/ExecuteCommand_1_3.hpp>

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <chrono>
#include <memory>
#include <unordered_map>

namespace ocvsmd
{
namespace sdk
{
namespace svc
{
namespace node
{

class ExecCmdClient
{
public:
    using Ptr          = std::shared_ptr<ExecCmdClient>;
    using Spec         = common::svc::node::ExecCmdSpec;
    using NodeResponse = uavcan::node::ExecuteCommand_1_3::Response;
    using Result       = std::unordered_map<std::uint16_t, NodeResponse>;

    CETL_NODISCARD static Ptr make(cetl::pmr::memory_resource&           memory,
                                   const common::ipc::ClientRouter::Ptr& ipc_router,
                                   Spec::Request&&                       request,
                                   const std::chrono::microseconds       timeout);

    ExecCmdClient(ExecCmdClient&&)                 = delete;
    ExecCmdClient(const ExecCmdClient&)            = delete;
    ExecCmdClient& operator=(ExecCmdClient&&)      = delete;
    ExecCmdClient& operator=(const ExecCmdClient&) = delete;

    virtual ~ExecCmdClient() = default;

    CETL_NODISCARD virtual cetl::optional<int>    completed() const = 0;
    CETL_NODISCARD virtual cetl::optional<Result> takeResult()      = 0;

protected:
    ExecCmdClient() = default;

};  // ExecCmdClient

}  // namespace node
}  // namespace svc
}  // namespace sdk
}  // namespace ocvsmd

#endif  // OCVSMD_SDK_SVC_NODE_EXEC_CMD_CLIENT_HPP_INCLUDED
