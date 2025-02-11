//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_SDK_SVC_FILE_SERVER_POP_ROOT_CLIENT_HPP_INCLUDED
#define OCVSMD_SDK_SVC_FILE_SERVER_POP_ROOT_CLIENT_HPP_INCLUDED

#include "ipc/client_router.hpp"
#include "svc/file_server/pop_root_spec.hpp"

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

namespace ocvsmd
{
namespace sdk
{
namespace svc
{
namespace file_server
{

class PopRootClient
{
public:
    using Ptr  = std::shared_ptr<PopRootClient>;
    using Spec = common::svc::file_server::PopRootSpec;

    using Success = cetl::monostate;
    using Failure = int;  // `errno`-like error code
    using Result  = cetl::variant<Success, Failure>;

    CETL_NODISCARD static Ptr make(cetl::pmr::memory_resource&           memory,
                                   const common::ipc::ClientRouter::Ptr& ipc_router,
                                   const Spec::Request&                  request);

    PopRootClient(PopRootClient&&)                 = delete;
    PopRootClient(const PopRootClient&)            = delete;
    PopRootClient& operator=(PopRootClient&&)      = delete;
    PopRootClient& operator=(const PopRootClient&) = delete;

    virtual ~PopRootClient() = default;

    template <typename Receiver>
    void submit(Receiver&& receiver)
    {
        submitImpl([receive = std::forward<Receiver>(receiver)](Result&& result) mutable {
            //
            receive(std::move(result));
        });
    }

protected:
    PopRootClient() = default;

    virtual void submitImpl(std::function<void(Result&&)>&& receiver) = 0;

};  // PopRootClient

}  // namespace file_server
}  // namespace svc
}  // namespace sdk
}  // namespace ocvsmd

#endif  // OCVSMD_SDK_SVC_FILE_SERVER_POP_ROOT_CLIENT_HPP_INCLUDED
