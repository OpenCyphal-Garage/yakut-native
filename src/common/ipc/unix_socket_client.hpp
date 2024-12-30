//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_UNIX_SOCKET_CLIENT_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_UNIX_SOCKET_CLIENT_HPP_INCLUDED

#include "unix_socket_base.hpp"

#include <string>

namespace ocvsmd
{
namespace common
{
namespace ipc
{

class UnixSocketClient final : public UnixSocketBase
{
public:
    explicit UnixSocketClient(std::string socket_path);

    UnixSocketClient(UnixSocketClient&&)                 = delete;
    UnixSocketClient(const UnixSocketClient&)            = delete;
    UnixSocketClient& operator=(UnixSocketClient&&)      = delete;
    UnixSocketClient& operator=(const UnixSocketClient&) = delete;

    ~UnixSocketClient();

    bool connectToServer();

    template <typename Message>
    Failure sendMessage(const Message& message) const
    {
        return writeMessage(client_fd_, message);
    }

private:
    std::string socket_path_;
    int         client_fd_;

};  // UnixSocketClient

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_UNIX_SOCKET_CLIENT_HPP_INCLUDED
