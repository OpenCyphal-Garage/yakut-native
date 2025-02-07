//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_SDK_DAEMON_HPP_INCLUDED
#define OCVSMD_SDK_DAEMON_HPP_INCLUDED

// ➕ #include "node_command_client.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/executor.hpp>

#include <memory>
#include <string>

namespace ocvsmd
{
namespace sdk
{

/// An abstract factory for the specialized interfaces.
///
class Daemon
{
public:
    using Ptr = std::shared_ptr<Daemon>;

    CETL_NODISCARD static Ptr make(cetl::pmr::memory_resource& memory,
                                   libcyphal::IExecutor&       executor,
                                   const std::string&          connection);

    Daemon(Daemon&&)                 = delete;
    Daemon(const Daemon&)            = delete;
    Daemon& operator=(Daemon&&)      = delete;
    Daemon& operator=(const Daemon&) = delete;

    virtual ~Daemon() = default;

    // ➕ virtual NodeCommandClient::Ptr getNodeCommandClient() = 0;

protected:
    Daemon() = default;

};  // Daemon

}  // namespace sdk
}  // namespace ocvsmd

#endif  // OCVSMD_SDK_DAEMON_HPP_INCLUDED
