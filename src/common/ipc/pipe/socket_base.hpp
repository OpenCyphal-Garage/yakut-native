//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_SOCKET_BASE_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_SOCKET_BASE_HPP_INCLUDED

#include "ipc/ipc_types.hpp"
#include "logging.hpp"

#include <cetl/cetl.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

class SocketBase
{
public:
    struct State final
    {
        enum class ReadPhase : std::uint8_t
        {
            Header,
            Payload
        };

        int         fd{-1};
        std::size_t read_msg_size{0};
        ReadPhase   read_phase{ReadPhase::Header};

    };  // State

    SocketBase(const SocketBase&)                = delete;
    SocketBase(SocketBase&&) noexcept            = delete;
    SocketBase& operator=(const SocketBase&)     = delete;
    SocketBase& operator=(SocketBase&&) noexcept = delete;

protected:
    SocketBase()  = default;
    ~SocketBase() = default;

    Logger& logger() const noexcept
    {
        return *logger_;
    }

    CETL_NODISCARD static int send(const State& state, const Payloads payloads);

    CETL_NODISCARD int receiveMessage(State& state, std::function<int(Payload)>&& action) const;

private:
    LoggerPtr logger_{getLogger("ipc")};

};  // SocketBase

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_SOCKET_BASE_HPP_INCLUDED
