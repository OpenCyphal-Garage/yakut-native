//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "socket_base.hpp"

#include "ipc/ipc_types.hpp"
#include "ocvsmd/platform/posix_utils.hpp"

#include <cetl/pf20/cetlpf.hpp>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{
namespace
{

struct MsgHeader final
{
    std::uint32_t signature;
    std::uint32_t size;
};

constexpr std::size_t   MsgSmallPayloadSize = 256;
constexpr std::uint32_t MsgSignature        = 0x5356434F;     // 'OCVS'
constexpr std::size_t   MsgMaxSize          = 1ULL << 20ULL;  // 1 MB

}  // namespace

int SocketBase::send(const State& state, const Payloads payloads)
{
    // 1. Write the message header (signature and total size of the following fragments).
    //
    const std::size_t total_size = std::accumulate(  // NOLINT
        payloads.begin(),
        payloads.end(),
        0ULL,
        [](const std::size_t acc, const Payload payload) {
            //
            return acc + payload.size();
        });
    if (const int err = platform::posixSyscallError([total_size, &state] {
            //
            const MsgHeader msg_header{MsgSignature, static_cast<std::uint32_t>(total_size)};
            return ::write(state.fd, &msg_header, sizeof(msg_header));
        }))
    {
        return err;
    }

    // 2. Write the message payload fragments.
    //
    for (const auto payload : payloads)
    {
        if (const int err = platform::posixSyscallError([payload, &state] {
                //
                return ::write(state.fd, payload.data(), payload.size());
            }))
        {
            return err;
        }
    }
    return 0;
}

int SocketBase::receiveMessage(State& state, std::function<int(Payload)>&& action) const
{
    // 1. Receive and validate the message header.
    //
    if (state.read_phase == State::ReadPhase::Header)
    {
        MsgHeader msg_header{};
        ssize_t   bytes_read = 0;
        if (const auto err = platform::posixSyscallError([&state, &msg_header, &bytes_read] {
                //
                return bytes_read = ::read(state.fd, &msg_header, sizeof(msg_header));
            }))
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                return 0;  // no data available yet
            }
            logger_->error("Failed to read message header (fd={}): {}.", state.fd, std::strerror(err));
            return err;
        }

        if (bytes_read == 0)
        {
            return -1;  // EOF
        }

        if ((bytes_read != sizeof(msg_header)) || (msg_header.signature != MsgSignature)  //
            || (msg_header.size == 0) || (msg_header.size > MsgMaxSize))
        {
            return EINVAL;
        }

        state.read_msg_size = msg_header.size;
        state.read_phase    = State::ReadPhase::Payload;
    }

    // 2. Read message payload.
    //
    if (state.read_phase == State::ReadPhase::Payload)
    {
        auto read_and_act = [this, &state, act = std::move(action)](  //
                                const cetl::span<std::uint8_t> buf_span) {
            //
            ssize_t read = 0;
            if (const auto err = platform::posixSyscallError([this, &state, buf_span, &read] {
                    //
                    return read = ::read(state.fd, buf_span.data(), buf_span.size());
                }))
            {
                if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                {
                    return 0;  // no data available
                }
                logger_->error("Failed to read message payload (fd={}): {}.", state.fd, std::strerror(err));
                return err;
            }
            if (read != buf_span.size())
            {
                return EINVAL;
            }

            state.read_phase = State::ReadPhase::Header;

            const cetl::span<const std::uint8_t> const_buf_span{buf_span};
            return act(const_buf_span);
        };
        if (state.read_msg_size <= MsgSmallPayloadSize)  // on stack buffer?
        {
            std::array<std::uint8_t, MsgSmallPayloadSize> buffer;  // NOLINT(*-member-init)
            return read_and_act({buffer.data(), state.read_msg_size});
        }

        // NOLINTNEXTLINE(*-avoid-c-arrays)
        const std::unique_ptr<std::uint8_t[]> buffer{new std::uint8_t[state.read_msg_size]};
        return read_and_act({buffer.get(), state.read_msg_size});
    }

    return 0;
}

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
