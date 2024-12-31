//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/sdk/daemon.hpp>

#include "ipc/unix_socket_client.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <cstdint>
#include <memory>
#include <unistd.h>
#include <utility>

namespace ocvsmd
{
namespace sdk
{
namespace
{

class DaemonImpl final : public Daemon
{
public:
    explicit DaemonImpl(cetl::pmr::memory_resource& memory)
        : memory_{memory}
    {
    }

    bool connect()
    {
        return ipc_client_.connectToServer();
    }

    void send_messages() const override
    {
        ipc_client_.sendMessage({reinterpret_cast<const std::uint8_t*>("Abc"), 3});  // NOLINT
        ::sleep(1);

        ipc_client_.sendMessage({reinterpret_cast<const std::uint8_t*>("xyZ"), 3});  // NOLINT
        ::sleep(1);
    }

private:
    cetl::pmr::memory_resource&   memory_;
    common::ipc::UnixSocketClient ipc_client_{"/var/run/ocvsmd/local.sock"};

};  // DaemonImpl

}  // namespace

std::unique_ptr<Daemon> Daemon::make(cetl::pmr::memory_resource& memory)
{
    auto daemon = std::make_unique<DaemonImpl>(memory);
    if (!daemon->connect())
    {
        return nullptr;
    }

    return std::move(daemon);
}

}  // namespace sdk
}  // namespace ocvsmd
