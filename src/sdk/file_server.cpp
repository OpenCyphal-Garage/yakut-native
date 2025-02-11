//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <ocvsmd/sdk/file_server.hpp>

#include "ipc/channel.hpp"
#include "ipc/client_router.hpp"
#include "logging.hpp"
#include "sdk_factory.hpp"
#include "svc/file_server/list_roots_client.hpp"

namespace ocvsmd
{
namespace sdk
{
namespace
{

class FileServerImpl final : public FileServer
{
public:
    FileServerImpl(cetl::pmr::memory_resource& memory, common::ipc::ClientRouter::Ptr ipc_router)
        : memory_{memory}
        , ipc_router_{std::move(ipc_router)}
        , logger_{common::getLogger("sdk")}
    {
    }

    // FileServer

    SenderOf<ListRoots::Result>::Ptr listRoots() override
    {
        const common::svc::file_server::ListRootsSpec::Request request{&memory_};
        auto svc_client = ListRootsClient::make(memory_, ipc_router_, request);

        return std::make_unique<ListRootsSender>(std::move(svc_client));
    }

private:
    using ListRootsClient = svc::file_server::ListRootsClient;

    class ListRootsSender final : public SenderOf<ListRoots::Result>
    {
    public:
        explicit ListRootsSender(ListRootsClient::Ptr svc_client)
            : svc_client_{std::move(svc_client)}
        {
        }

        void submitImpl(std::function<void(ListRoots::Result&&)>&& receiver) override
        {
            svc_client_->submit([receiver = std::move(receiver)](ListRootsClient::Result&& result) mutable {
                //
                receiver(std::move(result));
            });
        }

    private:
        ListRootsClient::Ptr svc_client_;

    };  // ListRootsSender

    cetl::pmr::memory_resource&    memory_;
    common::LoggerPtr              logger_;
    common::ipc::ClientRouter::Ptr ipc_router_;

};  // FileServerImpl

}  // namespace

CETL_NODISCARD FileServer::Ptr Factory::makeFileServer(cetl::pmr::memory_resource&    memory,
                                                       common::ipc::ClientRouter::Ptr ipc_router)
{
    return std::make_shared<FileServerImpl>(memory, std::move(ipc_router));
}

}  // namespace sdk
}  // namespace ocvsmd
