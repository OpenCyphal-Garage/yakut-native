//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_UNIX_SOCKET_BASE_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_UNIX_SOCKET_BASE_HPP_INCLUDED

#include "dsdl_helpers.hpp"
#include "platform/posix_utils.hpp"

#include <nunavut/support/serialization.hpp>

#include <cstddef>
#include <sys/syslog.h>
#include <unistd.h>

namespace ocvsmd
{
namespace common
{
namespace ipc
{

class UnixSocketBase
{
public:
    // Either POSIX errno or a Nunavut one.
    using Failure = cetl::variant<int, nunavut::support::Error>;

protected:
    template <typename Message>
    static Failure writeMessage(const int output_fd, const Message& message)
    {
        constexpr std::size_t BufferSize = Message::_traits_::SerializationBufferSizeBytes;
        constexpr bool        IsOnStack  = BufferSize <= MsgSmallPayloadSize;

        return tryPerformOnSerialized<Message, Failure, BufferSize, IsOnStack>(  //
            message,
            [output_fd](const auto msg_bytes_span) {
                //
                // 1. Write the message header.
                if (const int err = platform::posixSyscallError([output_fd, msg_bytes_span] {
                        //
                        const MsgHeader msg_header{.signature = MsgSignature,
                                                   .size      = static_cast<std::uint32_t>(msg_bytes_span.size())};
                        return ::write(output_fd, &msg_header, sizeof(msg_header));
                    }))
                {
                    return err;
                }
                // 2. Write the message payload.
                return platform::posixSyscallError([output_fd, msg_bytes_span] {
                    //
                    return ::write(output_fd, msg_bytes_span.data(), msg_bytes_span.size());
                });
            });
    }

    template <typename Action>
    static int readAndActOnMessage(const int input_fd, Action&& action)
    {
        // 1. Read and validate the message header.
        //
        std::size_t msg_size = 0;
        {
            MsgHeader msg_header;
            ssize_t bytes_read = 0;
            if (const auto err = platform::posixSyscallError([input_fd, &msg_header, &bytes_read] {
                    //
                    return bytes_read = ::read(input_fd, &msg_header, sizeof(msg_header));
                }))
            {
                ::syslog(LOG_ERR, "Failed to read message header (fd=%d): %s", input_fd, ::strerror(err));
                return err;
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
                ::syslog(LOG_ERR, "Failed to read message payload (fd=%d): %s", input_fd, ::strerror(err));
                return err;
            }
            if (read != buf_span.size())
            {
                return EINVAL;
            }

            const cetl::span<const std::uint8_t> const_buf_span{buf_span};
            return std::forward<Action>(act)(const_buf_span);
        };
        if (msg_size <= MsgSmallPayloadSize)  // on stack buffer?
        {
            std::array<std::uint8_t, MsgSmallPayloadSize> buffer;
            return read_and_act({buffer.data(), msg_size});
        }
        const std::unique_ptr<std::uint8_t[]> buffer{new std::uint8_t[msg_size]};
        return read_and_act({buffer.get(), msg_size});
    }

    template <typename Message>
    static Failure readMessage(const int input_fd, Message& message)
    {
        // 1. Read and validate the message header.
        //
        std::size_t msg_size = 0;
        {
            MsgHeader msg_header;
            ssize_t bytes_read = 0;
            if (const auto err = platform::posixSyscallError([input_fd, &msg_header, &bytes_read] {
                    //
                    return bytes_read = ::read(input_fd, &msg_header, sizeof(msg_header));
                }))
            {
                return err;
            }

            // 2. Validate message header.
            if ((bytes_read != sizeof(msg_header)) || (msg_header.signature != MsgSignature)  //
                || (msg_header.size == 0) || (msg_header.size > MsgMaxSize))
            {
                return EINVAL;
            }

            msg_size = msg_header.size;
        }

        // 2. Deserialize message payload.
        //
        auto read_payload_and_deser_msg = [input_fd, &message](const cetl::span<std::uint8_t> buf_span) {
            //
            ssize_t read = 0;
            if (const auto err = platform::posixSyscallError([input_fd, buf_span, &read] {
                    //
                    return read = ::read(input_fd, buf_span.data(), buf_span.size());
                }))
            {
                return err;
            }
            if (read != buf_span.size())
            {
                return EINVAL;
            }
            return tryDeserializePayload({buf_span.data(), buf_span.size()}, message);
        };
        if (msg_size <= MsgSmallPayloadSize)  // on stack buffer?
        {
            std::array<std::uint8_t, MsgSmallPayloadSize> buffer;
            return read_payload_and_deser_msg({buffer.data(), msg_size});
        }
        const std::unique_ptr<std::uint8_t[]> buffer{new std::uint8_t[msg_size]};
        return read_payload_and_deser_msg({buffer.get(), msg_size});
    }

private:
    struct MsgHeader
    {
        std::uint32_t signature;
        std::uint32_t size;
    };

    static constexpr std::size_t   MsgSmallPayloadSize = 256;
    static constexpr std::uint32_t MsgSignature        = 0x5356434F;     // 'OCVS'
    static constexpr std::size_t   MsgMaxSize          = 1ULL << 20ULL;  // 1 MB

};  // UnixSocketBase

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_UNIX_SOCKET_BASE_HPP_INCLUDED
