//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_SERVER_PIPE_MOCK_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_SERVER_PIPE_MOCK_HPP_INCLUDED

#include "ipc/pipe/server_pipe.hpp"

#include "ipc/pipe/pipe_types.hpp"
#include "unique_ptr_refwrapper.hpp"

#include <gmock/gmock.h>

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
            reference().event_handler_ = event_handler;
            return reference().start(event_handler);
        }
        int sendMessage(const ClientId client_id, const Payloads payloads) override
        {
            return reference().sendMessage(client_id, payloads);
        }

    };  // RefWrapper

    MOCK_METHOD(void, deinit, (), (const));
    MOCK_METHOD(int, start, (EventHandler event_handler), (override));
    MOCK_METHOD(int, sendMessage, (const ClientId client_id, const Payloads payloads), (override));

    // MARK: Data members:

    // NOLINTBEGIN
    EventHandler event_handler_;
    // NOLINTEND

};  // ServerPipeMock

}  // namespace pipe
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_SERVER_PIPE_MOCK_HPP_INCLUDED
