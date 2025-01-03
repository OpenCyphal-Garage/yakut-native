//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "client_router.hpp"

#include <memory>

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
    ClientRouterImpl() = default;

};  // ClientRouterImpl

}  // namespace

std::unique_ptr<ClientRouter> ClientRouter::make()
{
    return std::make_unique<ClientRouterImpl>();
}

}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd
