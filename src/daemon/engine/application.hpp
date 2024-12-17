//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_ENGINE_APPLICATION_HPP_INCLUDED
#define OCVSMD_DAEMON_ENGINE_APPLICATION_HPP_INCLUDED

#include "platform/single_threaded_executor.hpp"

#include <cetl/pf17/cetlpf.hpp>

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
    void                                       runWith(const std::function<bool()>& loop_predicate);

private:
    platform::SingleThreadedExecutor executor_;

};  // Application

}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd

#endif  // OCVSMD_DAEMON_ENGINE_APPLICATION_HPP_INCLUDED
