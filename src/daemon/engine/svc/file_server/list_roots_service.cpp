//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "list_roots_service.hpp"

#include "cyphal/file_provider.hpp"
#include "ipc/channel.hpp"
#include "ipc/server_router.hpp"
#include "logging.hpp"
#include "svc/file_server/list_roots_spec.hpp"
#include "svc/svc_helpers.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <memory>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{
namespace svc
{
namespace file_server
{
namespace
{

class ListRootServiceImpl final
{
public:
    using Spec    = common::svc::file_server::ListRootsSpec;
    using Channel = common::ipc::Channel<Spec::Request, Spec::Response>;

    explicit ListRootServiceImpl(const ScvContext& context, cyphal::FileProvider& file_provider)
        : context_{context}
        , file_provider_{file_provider}
    {
    }

    void operator()(Channel channel, const Spec::Request&) const
    {
        logger_->debug("New '{}' service channel.", Spec::svc_full_name());

        Spec::Response ipc_response{&context_.memory};
        const auto&    roots = file_provider_.getListOfRoots();
        for (const auto& root : roots)
        {
            constexpr auto MaxRootLen = Spec::Response::_traits_::TypeOf::item::_traits_::ArrayCapacity::path;
            if (root.size() > MaxRootLen)
            {
                logger_->warn("ListRootSvc: Can't list too long path (max_len={}, root='{}').", MaxRootLen, root);
                continue;
            }

            ipc_response.item.path.clear();
            std::copy(root.cbegin(), root.cend(), std::back_inserter(ipc_response.item.path));
            if (const auto err = channel.send(ipc_response))
            {
                logger_->warn("ListRootSvc: failed to send ipc response (err={}).", err);
            }
        }

        channel.complete(0);
    }

private:
    const ScvContext      context_;
    cyphal::FileProvider& file_provider_;
    common::LoggerPtr     logger_{common::getLogger("engine")};

};  // ExecCmdServiceImpl

}  // namespace

void ListRootService::registerWithContext(const ScvContext& context, cyphal::FileProvider& file_provider)
{
    using Impl = ListRootServiceImpl;
    context.ipc_router.registerChannel<Impl::Channel>(Impl::Spec::svc_full_name(), Impl{context, file_provider});
}

}  // namespace file_server
}  // namespace svc
}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd
