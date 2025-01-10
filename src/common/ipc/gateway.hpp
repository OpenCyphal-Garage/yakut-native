//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_IPC_GATEWAY_HPP_INCLUDED
#define OCVSMD_COMMON_IPC_GATEWAY_HPP_INCLUDED

#include "ipc_types.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf20/cetlpf.hpp>

#include <cstdint>
#include <functional>
#include <memory>

namespace ocvsmd
{
namespace common
{
namespace ipc
{
namespace detail
{

using ServiceId = std::uint64_t;

class Gateway
{
public:
    using Ptr     = std::shared_ptr<Gateway>;
    using WeakPtr = std::weak_ptr<Gateway>;

    struct Event final
    {
        struct Connected final
        {};
        struct Completed final
        {
            ErrorCode error_code;
        };
        struct Message final
        {
            std::uint64_t sequence;
            Payload       payload;
        };

        using Var = cetl::variant<Connected, Message, Completed>;

    };  // Event

    using EventHandler = std::function<int(const Event::Var&)>;

    Gateway(const Gateway&)                = delete;
    Gateway(Gateway&&) noexcept            = delete;
    Gateway& operator=(const Gateway&)     = delete;
    Gateway& operator=(Gateway&&) noexcept = delete;

    CETL_NODISCARD virtual int send(const ServiceId service_id, const Payload payload) = 0;
    CETL_NODISCARD virtual int event(const Event::Var& event)                          = 0;
    virtual void               subscribe(EventHandler event_handler)                   = 0;

protected:
    Gateway()  = default;
    ~Gateway() = default;

};  // Gateway

}  // namespace detail
}  // namespace ipc
}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_IPC_GATEWAY_HPP_INCLUDED
