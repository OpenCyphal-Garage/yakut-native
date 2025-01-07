//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_BASE_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_BASE_HPP_INCLUDED

#include "ocvsmd/platform/posix_utils.hpp"
#include "pipe_types.hpp"

#include <cetl/pf20/cetlpf.hpp>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <sys/syslog.h>
#include <sys/types.h>
#include <unistd.h>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

class UnixSocketBase
{
public:
    UnixSocketBase(const UnixSocketBase&)                = delete;
    UnixSocketBase(UnixSocketBase&&) noexcept            = delete;
    UnixSocketBase& operator=(const UnixSocketBase&)     = delete;
    UnixSocketBase& operator=(UnixSocketBase&&) noexcept = delete;

protected:
    UnixSocketBase()  = default;
    ~UnixSocketBase() = default;

    static int sendMessage(const int output_fd, const Payloads payloads)
    {
        // 1. Write the message header (signature and total size of the following fragments).
        //
        const std::size_t total_size = std::accumulate(  //
            payloads.begin(),
            payloads.end(),
            0ULL,
            [](const std::size_t acc, const Payload payload) {
                //
                return acc + payload.size();
            });
        if (const int err = platform::posixSyscallError([output_fd, total_size] {
                //
                const MsgHeader msg_header{MsgSignature, static_cast<std::uint32_t>(total_size)};
                return ::write(output_fd, &msg_header, sizeof(msg_header));
            }))
        {
            return err;
        }

        // 2. Write the message payload fragments.
        //
        for (const auto payload : payloads)
        {
            if (const int err = platform::posixSyscallError([output_fd, payload] {
                    //
                    return ::write(output_fd, payload.data(), payload.size());
                }))
            {
                return err;
            }
        }
        return 0;
    }

    template <typename Action>
    static int receiveMessage(const int input_fd, Action&& action)
    {
        // 1. Receive and validate the message header.
        //
        std::size_t msg_size = 0;
        {
            MsgHeader msg_header;
            ssize_t   bytes_read = 0;
            if (const auto err = platform::posixSyscallError([input_fd, &msg_header, &bytes_read] {
                    //
                    return bytes_read = ::read(input_fd, &msg_header, sizeof(msg_header));
                }))
            {
                // NOLINTNEXTLINE *-vararg
                ::syslog(LOG_ERR, "Failed to read message header (fd=%d): %s", input_fd, std::strerror(err));
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

            msg_size = msg_header.size;
        }

        // 2. Read message payload.
        //
        auto read_and_act = [input_fd, act = std::forward<Action>(action)](const cetl::span<std::uint8_t> buf_span) {
            //
            ssize_t read = 0;
            if (const auto err = platform::posixSyscallError([input_fd, buf_span, &read] {
                    //
                    return read = ::read(input_fd, buf_span.data(), buf_span.size());
                }))
            {
                // NOLINTNEXTLINE *-vararg
                ::syslog(LOG_ERR, "Failed to read message payload (fd=%d): %s", input_fd, std::strerror(err));
                return err;
            }
            if (read != buf_span.size())
            {
                return EINVAL;
            }

            const cetl::span<const std::uint8_t> const_buf_span{buf_span};
            return act(const_buf_span);
        };
        if (msg_size <= MsgSmallPayloadSize)  // on stack buffer?
        {
            std::array<std::uint8_t, MsgSmallPayloadSize> buffer;
            return read_and_act({buffer.data(), msg_size});
        }
        const std::unique_ptr<std::uint8_t[]> buffer{new std::uint8_t[msg_size]};
        return read_and_act({buffer.get(), msg_size});
    }

private:
    struct MsgHeader final
    {
        std::uint32_t signature;
        std::uint32_t size;
    };

    static constexpr std::size_t   MsgSmallPayloadSize = 256;
    static constexpr std::uint32_t MsgSignature        = 0x5356434F;     // 'OCVS'
    static constexpr std::size_t   MsgMaxSize          = 1ULL << 20ULL;  // 1 MB

};  // UnixSocketBase

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_PIPE_UNIX_SOCKET_BASE_HPP_INCLUDED
