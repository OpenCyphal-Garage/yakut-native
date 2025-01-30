//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_SDK_NODE_COMMAND_CLIENT_HPP_INCLUDED
#define OCVSMD_SDK_NODE_COMMAND_CLIENT_HPP_INCLUDED

#include "execution.hpp"

#include <uavcan/node/ExecuteCommand_1_3.hpp>

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace ocvsmd
{
namespace sdk
{

class NodeCommandClient
{
public:
    using Ptr = std::shared_ptr<NodeCommandClient>;

    NodeCommandClient(NodeCommandClient&&)                 = delete;
    NodeCommandClient(const NodeCommandClient&)            = delete;
    NodeCommandClient& operator=(NodeCommandClient&&)      = delete;
    NodeCommandClient& operator=(const NodeCommandClient&) = delete;

    virtual ~NodeCommandClient() = default;

    struct Command final
    {
        using NodeRequest  = uavcan::node::ExecuteCommand_1_3::Request;
        using NodeResponse = uavcan::node::ExecuteCommand_1_3::Response;

        using Success = std::unordered_map<std::uint16_t, NodeResponse>;
        using Failure = int;  // `errno`-like error code.
        using Result  = cetl::variant<Success, Failure>;

    };  // Command

    /// Request is sent concurrently to all nodes.
    /// Duplicate node IDs are ignored.
    /// Result will be available when the last response has arrived,
    /// or the timeout has expired.
    ///
    virtual SenderOf<Command::Result>::Ptr sendCommand(const cetl::span<const std::uint16_t> node_ids,
                                                       const Command::NodeRequest&           node_request,
                                                       const std::chrono::microseconds       timeout) = 0;

``    /// A convenience method for invoking `sendCommand` with COMMAND_RESTART.
    ///
    SenderOf<Command::Result>::Ptr restart(  //
        const cetl::span<const std::uint16_t> node_ids,
        const std::chrono::microseconds       timeout = std::chrono::seconds{1});

    /// A convenience method for invoking `sendCommand` with COMMAND_BEGIN_SOFTWARE_UPDATE.
    /// The file_path is relative to one of the roots configured in the file server.
    ///
    SenderOf<Command::Result>::Ptr beginSoftwareUpdate(  //
        const cetl::span<const std::uint16_t> node_ids,
        const cetl::string_view               file_path,
        const std::chrono::microseconds       timeout = std::chrono::seconds{1});

protected:
    NodeCommandClient() = default;

    virtual cetl::pmr::memory_resource& getMemoryResource() const noexcept = 0;

};  // NodeCommandClient

}  // namespace sdk
}  // namespace ocvsmd

#endif  // OCVSMD_SDK_NODE_COMMAND_CLIENT_HPP_INCLUDED
