//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "server_router.hpp"

#include "pipe/server_pipe.hpp"

#include <cetl/cetl.hpp>

#include <memory>
#include <utility>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace
{

class ServerRouterImpl final : public ServerRouter
{
public:
    explicit ServerRouterImpl(pipe::ServerPipe::Ptr server_pipe)
        : server_pipe_{std::move(server_pipe)}
    {
        CETL_DEBUG_ASSERT(server_pipe_, "");
    }

private:
    pipe::ServerPipe::Ptr server_pipe_;

};  // ClientRouterImpl

}  // namespace

ServerRouter::Ptr ServerRouter::make(pipe::ServerPipe::Ptr server_pipe)
{
    return std::make_unique<ServerRouterImpl>(std::move(server_pipe));
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
