//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_UNIX_SOCKET_SERVER_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_UNIX_SOCKET_SERVER_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <libcyphal/executor.hpp>

#include <string>

namespace ocvsmd
{
namespace common
{
namespace ipc
{

class UnixSocketServer final
{
public:
    UnixSocketServer(libcyphal::IExecutor& executor, std::string socket_path);

    UnixSocketServer(UnixSocketServer&&)                 = delete;
    UnixSocketServer(const UnixSocketServer&)            = delete;
    UnixSocketServer& operator=(UnixSocketServer&&)      = delete;
    UnixSocketServer& operator=(const UnixSocketServer&) = delete;

    ~UnixSocketServer();

    bool start();

    CETL_NODISCARD libcyphal::IExecutor::Callback::Any registerListenCallback(
        libcyphal::IExecutor::Callback::Function&& function) const;

    void accept();

private:
    void handle_client(int client_fd);

    libcyphal::IExecutor& executor_;
    std::string           socket_path_;
    int                   server_fd_;

};  // UnixSocketServer

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_UNIX_SOCKET_SERVER_HPP_INCLUDED
