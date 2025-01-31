//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "file_provider.hpp"

#include "engine_helpers.hpp"
#include "logging.hpp"

#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/presentation/presentation.hpp>
#include <libcyphal/presentation/server.hpp>
#include <libcyphal/types.hpp>

#include <uavcan/file/Error_1_0.hpp>
#include <uavcan/file/GetInfo_0_2.hpp>
#include <uavcan/file/List_0_2.hpp>
#include <uavcan/file/Modify_1_1.hpp>
#include <uavcan/file/Path_2_0.hpp>
#include <uavcan/file/Read_1_1.hpp>
#include <uavcan/file/Write_1_1.hpp>

#include <spdlog/spdlog.h>

#include <memory>
#include <utility>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{
namespace cyphal
{
namespace
{

class FileProviderImpl final : public FileProvider
{
    template <typename T>
    struct SvcSpec : T
    {
        using Server      = libcyphal::presentation::ServiceServer<T>;
        using CallbackArg = typename Server::OnRequestCallback::Arg;
    };
    struct Svc
    {
        using List    = SvcSpec<uavcan::file::List_0_2>;
        using Read    = SvcSpec<uavcan::file::Read_1_1>;
        using Write   = SvcSpec<uavcan::file::Write_1_1>;
        using Modify  = SvcSpec<uavcan::file::Modify_1_1>;
        using GetInfo = SvcSpec<uavcan::file::GetInfo_0_2>;

    };  // Svc

public:
    static Ptr make(cetl::pmr::memory_resource& memory, libcyphal::presentation::Presentation& presentation)
    {
        auto list_srv     = makeServer<Svc::List>("List", presentation);
        auto read_srv     = makeServer<Svc::Read>("Read", presentation);
        auto write_srv    = makeServer<Svc::Write>("Write", presentation);
        auto modify_srv   = makeServer<Svc::Modify>("Modify", presentation);
        auto get_info_srv = makeServer<Svc::GetInfo>("GetInfo", presentation);
        if (!list_srv || !read_srv || !write_srv || !modify_srv || !get_info_srv)
        {
            return nullptr;
        }
        return std::make_unique<FileProviderImpl>(memory,
                                                  std::move(*list_srv),
                                                  std::move(*read_srv),
                                                  std::move(*write_srv),
                                                  std::move(*modify_srv),
                                                  std::move(*get_info_srv));
    }

    FileProviderImpl(cetl::pmr::memory_resource& memory,
                     Svc::List::Server&&         list_srv,
                     Svc::Read::Server&&         read_srv,
                     Svc::Write::Server&&        write_srv,
                     Svc::Modify::Server&&       modify_srv,
                     Svc::GetInfo::Server&&      get_info_srv)
        : memory_{memory}
        , list_srv_{std::move(list_srv)}
        , read_srv_{std::move(read_srv)}
        , write_srv_{std::move(write_srv)}
        , modify_srv_{std::move(modify_srv)}
        , get_info_srv_{std::move(get_info_srv)}
    {
        logger_->trace("FileProviderImpl().");

        setupOnRequestCallback<Svc::GetInfo>(get_info_srv_, [this](const auto& arg) {
            //
            return serveGetInfoRequest(arg);
        });
        setupOnRequestCallback<Svc::Read>(read_srv_, [this](const auto& arg) {
            //
            return serveReadRequest(arg);
        });
    }

    FileProviderImpl(const FileProviderImpl&)                = delete;
    FileProviderImpl(FileProviderImpl&&) noexcept            = delete;
    FileProviderImpl& operator=(const FileProviderImpl&)     = delete;
    FileProviderImpl& operator=(FileProviderImpl&&) noexcept = delete;

    ~FileProviderImpl() override
    {
        logger_->trace("~FileProviderImpl.");
    }

    // FileProvider

private:
    using Presentation = libcyphal::presentation::Presentation;

    template <typename Service>
    static auto makeServer(const cetl::string_view role, Presentation& presentation)
        -> cetl::optional<typename Service::Server>
    {
        auto maybe_server = presentation.makeServer<Service>();
        if (const auto* const failure = cetl::get_if<Presentation::MakeFailure>(&maybe_server))
        {
            const auto err = failureToErrorCode(*failure);
            spdlog::error("Failed to make '{}' server (err={}).", role, err);
            return cetl::nullopt;
        }
        return cetl::get<typename Service::Server>(std::move(maybe_server));
    }

    template <typename Service, typename Handler>
    void setupOnRequestCallback(typename Service::Server& server, Handler&& handler)
    {
        server.setOnRequestCallback([handle = std::forward<Handler>(handler)](const auto& arg, auto& continuation) {
            //
            constexpr auto timeout = std::chrono::seconds{1};
            continuation(arg.approx_now + timeout, handle(arg));
        });
    }

    static cetl::string_view stringViewFrom(const uavcan::file::Path_2_0& file_path)
    {
        const auto* const path_data = reinterpret_cast<const char*>(file_path.path.data());  // NOLINT
        return cetl::string_view{path_data, file_path.path.size()};
    }

    static constexpr std::size_t TestFileSize = 1000000000;

    Svc::GetInfo::Response serveGetInfoRequest(const Svc::GetInfo::CallbackArg& arg)
    {
        const auto request_path = stringViewFrom(arg.request.path);
        logger_->trace("'GetInfo' request (from={}, path='{}').", arg.metadata.remote_node_id, request_path);

        Svc::GetInfo::Response response{&memory_};
        response._error.value          = uavcan::file::Error_1_0::OK;
        response.size                  = TestFileSize;
        response.is_file_not_directory = true;
        response.is_readable           = true;
        return response;
    }

    Svc::Read::Response serveReadRequest(const Svc::Read::CallbackArg& arg)
    {
        // const auto request_path = stringViewFrom(arg.request.path);
        // logger_->trace("'Read' request (from={}, offset={}, path='{}').",
        //                arg.metadata.remote_node_id,
        //                arg.request.offset,
        //                request_path);

        Svc::Read::Response response{&memory_};
        response._error.value = uavcan::file::Error_1_0::OK;
        response.data.value.resize(std::min<std::size_t>(TestFileSize - arg.request.offset, 256U));  // NOLINT
        return response;
    }

    static constexpr libcyphal::Duration Timeout = std::chrono::seconds{1};

    cetl::pmr::memory_resource& memory_;
    Svc::List::Server           list_srv_;
    Svc::Read::Server           read_srv_;
    Svc::Write::Server          write_srv_;
    Svc::Modify::Server         modify_srv_;
    Svc::GetInfo::Server        get_info_srv_;
    common::LoggerPtr           logger_{common::getLogger("engine")};

};  // FileProviderImpl

}  // namespace

FileProvider::Ptr FileProvider::make(cetl::pmr::memory_resource&            memory,
                                     libcyphal::presentation::Presentation& presentation)
{
    return FileProviderImpl::make(memory, presentation);
}

}  // namespace cyphal
}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd
