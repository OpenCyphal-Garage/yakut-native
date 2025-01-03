//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "client_router.hpp"

#include "pipe/client_pipe.hpp"

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

class ClientRouterImpl final : public ClientRouter
{
public:
    explicit ClientRouterImpl(pipe::ClientPipe::Ptr client_pipe)
        : client_pipe_{std::move(client_pipe)}
    {
        CETL_DEBUG_ASSERT(client_pipe_, "");
    }

private:
    pipe::ClientPipe::Ptr client_pipe_;

};  // ClientRouterImpl

}  // namespace

ClientRouter::Ptr ClientRouter::make(pipe::ClientPipe::Ptr client_pipe)
{
    return std::make_unique<ClientRouterImpl>(std::move(client_pipe));
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
