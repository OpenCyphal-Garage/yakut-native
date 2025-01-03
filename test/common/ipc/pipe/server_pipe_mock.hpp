//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_SERVER_PIPE_MOCK_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_SERVER_PIPE_MOCK_HPP_INCLUDED

#include "ipc/pipe/server_pipe.hpp"
#include "unique_ptr_refwrapper.hpp"

#include <gmock/gmock.h>

#include <functional>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace pipe
{

class ServerPipeMock : public ServerPipe
{
public:
    struct RefWrapper final : UniquePtrRefWrapper<ServerPipe, ServerPipeMock>
    {
        using UniquePtrRefWrapper::UniquePtrRefWrapper;

        // MARK: ServerPipe

        int start(EventHandler event_handler) override
        {
            return reference().start(event_handler);
        }
        int sendMessage(const ClientId client_id, const Payload payload) override
        {
            return reference().sendMessage(client_id, payload);
        }

    };  // RefWrapper

    MOCK_METHOD(void, deinit, (), (const));
    MOCK_METHOD(int, start, (EventHandler event_handler), (override));
    MOCK_METHOD(int, sendMessage, (const ClientId client_id, const Payload payload), (override));

};  // ServerPipeMock

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_SERVER_PIPE_MOCK_HPP_INCLUDED
