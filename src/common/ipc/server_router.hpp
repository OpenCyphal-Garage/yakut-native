//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_SERVER_ROUTER_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_SERVER_ROUTER_HPP_INCLUDED

#include "pipe/server_pipe.hpp"

#include <memory>

namespace ocvsmd
{
namespace common
{
namespace ipc
{

class ServerRouter
{
public:
    using Ptr = std::unique_ptr<ServerRouter>;

    static Ptr make(pipe::ServerPipe::Ptr server_pipe);

    ServerRouter(ServerRouter&&)                 = delete;
    ServerRouter(const ServerRouter&)            = delete;
    ServerRouter& operator=(ServerRouter&&)      = delete;
    ServerRouter& operator=(const ServerRouter&) = delete;

    virtual ~ServerRouter() = default;

protected:
    ServerRouter() = default;

};  // ServerRouter

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_SERVER_ROUTER_HPP_INCLUDED
