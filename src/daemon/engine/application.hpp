//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_ENGINE_APPLICATION_HPP_INCLUDED
#define OCVSMD_DAEMON_ENGINE_APPLICATION_HPP_INCLUDED

#include "cyphal/udp_transport_bag.hpp"
#include "logging.hpp"
#include "ocvsmd/platform/defines.hpp"

#include "ocvsmd/common/node_command/ExecCmd_0_1.hpp"

#include <ipc/channel.hpp>
#include <ipc/server_router.hpp>

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/application/node.hpp>
#include <libcyphal/presentation/presentation.hpp>

#include <uavcan/node/GetInfo_1_0.hpp>

#include <functional>
#include <string>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{

class Application
{
public:
    CETL_NODISCARD cetl::optional<std::string> init();
    void                                       runWhile(const std::function<bool()>& loop_predicate);

private:
    // TODO: temp stuff
    using ExecCmd        = common::node_command::ExecCmd_0_1;
    using ExecCmdChannel = common::ipc::Channel<ExecCmd, ExecCmd>;
    cetl::optional<ExecCmdChannel> ipc_exec_cmd_ch_;

    using UniqueId = uavcan::node::GetInfo::Response_1_0::_traits_::TypeOf::unique_id;

    static UniqueId getUniqueId();

    common::LoggerPtr                                     logger_{common::getLogger("engine")};
    ocvsmd::platform::SingleThreadedExecutor              executor_;
    cetl::pmr::memory_resource&                           memory_{*cetl::pmr::get_default_resource()};
    cyphal::UdpTransportBag                               udp_transport_bag_{memory_, executor_};
    cetl::optional<libcyphal::presentation::Presentation> presentation_;
    cetl::optional<libcyphal::application::Node>          node_;
    common::ipc::ServerRouter::Ptr                        ipc_router_;

};  // Application

}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd

#endif  // OCVSMD_DAEMON_ENGINE_APPLICATION_HPP_INCLUDED
