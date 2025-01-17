//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "exec_cmd_client.hpp"

#include "ipc/channel.hpp"
#include "ipc/client_router.hpp"
#include "logging.hpp"
#include "svc/node/exec_cmd_spec.hpp"

#include <uavcan/node/ExecuteCommand_1_3.hpp>

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace ocvsmd
{
namespace sdk
{
namespace svc
{
namespace node
{
namespace
{

class ExecCmdClientImpl final : public ExecCmdClient
{
public:
    ExecCmdClientImpl(cetl::pmr::memory_resource&           memory,
                      const common::ipc::ClientRouter::Ptr& ipc_router,
                      Spec::Request&&                       request,
                      const std::chrono::microseconds       timeout)
        : memory_{memory}
        , logger_{common::getLogger("svc")}
        , request_{std::move(request)}
        , channel_{ipc_router->makeChannel<Channel>(Spec::svc_full_name())}
    {
        channel_.subscribe([this](const auto& event_var) {
            //
            cetl::visit([this](const auto& event) { handleEvent(event); }, event_var);
        });

        // TODO: handle timeout
        (void) timeout;
    }

    CETL_NODISCARD cetl::optional<int> completed() const override
    {
        return completion_error_code_;
    }

    CETL_NODISCARD cetl::optional<Result> takeResult() override
    {
        if (!completion_error_code_.has_value())
        {
            return cetl::nullopt;
        }

        Result result;
        std::swap(result, node_id_to_response_);
        return result;
    }

private:
    using Channel = common::ipc::Channel<Spec::Response, Spec::Request>;

    void handleEvent(const Channel::Connected& connected)
    {
        logger_->trace("ExecCmdClient::handleEvent({}).", connected);

        if (const auto err = channel_.send(request_))
        {
            completion_error_code_.emplace(err);
        }
    }

    void handleEvent(const Channel::Input& input)
    {
        logger_->trace("ExecCmdClient::handleEvent(Input).");

        NodeResponse node_response{input.payload.status, input.payload.output, &memory_};
        node_id_to_response_.emplace(input.node_id, std::move(node_response));
    }

    void handleEvent(const Channel::Completed& completed)
    {
        logger_->debug("ExecCmdClient::handleEvent({}).", completed);
        completion_error_code_ = static_cast<int>(completed.error_code);
    }

    cetl::pmr::memory_resource&                     memory_;
    common::LoggerPtr                               logger_;
    Spec::Request                                   request_;
    Channel                                         channel_;
    std::unordered_map<std::uint16_t, NodeResponse> node_id_to_response_;
    cetl::optional<int>                             completion_error_code_;

};  // ExecCmdClientImpl

}  // namespace

CETL_NODISCARD ExecCmdClient::Ptr ExecCmdClient::make(cetl::pmr::memory_resource&           memory,
                                                      const common::ipc::ClientRouter::Ptr& ipc_router,
                                                      Spec::Request&&                       request,
                                                      const std::chrono::microseconds       timeout)
{
    return std::make_shared<ExecCmdClientImpl>(memory, ipc_router, std::move(request), timeout);
}

}  // namespace node
}  // namespace svc
}  // namespace sdk
}  // namespace ocvsmd
