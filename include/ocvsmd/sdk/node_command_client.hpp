//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_SDK_NODE_COMMAND_CLIENT_HPP_INCLUDED
#define OCVSMD_SDK_NODE_COMMAND_CLIENT_HPP_INCLUDED

#include <uavcan/node/ExecuteCommand_1_3.hpp>

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
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

        /// Empty option indicates that the corresponding node did not return a response on time.
        using Success = std::unordered_map<std::uint16_t, cetl::optional<NodeResponse>>;

        /// `errno`-like error code.
        using Failure = int;

        using Result        = cetl::variant<Success, Failure>;
        using ResultHandler = std::function<void(Result result)>;

    };  // Command

    /// All requests are sent concurrently.
    /// The callback is executed when the last response has arrived,
    /// or the timeout has expired.
    ///
    virtual int sendCommand(const cetl::span<const std::uint16_t> node_ids,
                            const Command::NodeRequest&           node_request,
                            const std::chrono::microseconds       timeout,
                            Command::ResultHandler                result_handler) = 0;

protected:
    NodeCommandClient() = default;

};  // NodeCommandClient

}  // namespace sdk
}  // namespace ocvsmd

#endif  // OCVSMD_SDK_NODE_COMMAND_CLIENT_HPP_INCLUDED
