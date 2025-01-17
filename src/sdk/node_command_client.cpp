//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/sdk/node_command_client.hpp>

#include "ipc/channel.hpp"
#include "ipc/client_router.hpp"
#include "logging.hpp"
#include "sdk_factory.hpp"
#include "svc/node/exec_cmd_spec.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <memory>
#include <utility>

namespace ocvsmd
{
namespace sdk
{
namespace
{

class NodeCommandClientImpl final : public NodeCommandClient
{
public:
    NodeCommandClientImpl(cetl::pmr::memory_resource& memory, common::ipc::ClientRouter::Ptr ipc_router)
        : memory_{memory}
        , ipc_router_{std::move(ipc_router)}
        , logger_{common::getLogger("sdk")}
    {
    }

    // NodeCommandClient

    int sendCommand(const cetl::span<const std::uint16_t> /* node_ids */,
                    const Command::NodeRequest& /* node_request */,
                    const std::chrono::microseconds /* timeout */,
                    Command::ResultHandler /* result_handler */) override
    {
        // using ExecCmdRequest = ExecCmdSvcSpec::Request;
        // using RequestPayload = ExecCmdSvcSpec::Request::_traits_::TypeOf::payload;
        //
        // if (node_ids.size() > ExecCmdSvcSpec::Request::_traits_::ArrayCapacity::node_ids)
        // {
        //     logger_->error("Too many node IDs: {} (max {}).",
        //                    node_ids.size(),
        //                    ExecCmdSvcSpec::Request::_traits_::ArrayCapacity::node_ids);
        //     return EINVAL;
        // }
        //
        // auto exec_cmd_ch = ipc_router_->makeChannel<ExecCmdChannel>(ExecCmdSvcSpec::svc_full_name);
        // exec_cmd_ch.subscribe([](const auto& event) {});
        //
        // const RequestPayload request_payload{node_request.command, node_request.parameter, &memory_};
        // const ExecCmdRequest exec_cmd_req{{node_ids.begin(), node_ids.end()}, request_payload, &memory_};
        //
        // return exec_cmd_ch.send(exec_cmd_req);

        return 0;
    }

private:
    using ExecCmdSvcSpec = common::svc::node::ExecCmdSpec;
    using ExecCmdChannel = common::ipc::Channel<ExecCmdSvcSpec::Response, ExecCmdSvcSpec::Request>;

    cetl::pmr::memory_resource&    memory_;
    common::LoggerPtr              logger_;
    common::ipc::ClientRouter::Ptr ipc_router_;

};  // NodeCommandClientImpl

}  // namespace

CETL_NODISCARD NodeCommandClient::Ptr Factory::makeNodeCommandClient(cetl::pmr::memory_resource&    memory,
                                                                     common::ipc::ClientRouter::Ptr ipc_router)
{
    return std::make_shared<NodeCommandClientImpl>(memory, std::move(ipc_router));
}

}  // namespace sdk
}  // namespace ocvsmd
