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

#include <uavcan/file/Error_1_0.hpp>
#include <uavcan/file/GetInfo_0_2.hpp>
#include <uavcan/file/List_0_2.hpp>
#include <uavcan/file/Modify_1_1.hpp>
#include <uavcan/file/Path_2_0.hpp>
#include <uavcan/file/Read_1_1.hpp>
#include <uavcan/file/Write_1_1.hpp>

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <sys/stat.h>
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

    static std::string stringFrom(const uavcan::file::Path_2_0& file_path)
    {
        const auto* const path_data = reinterpret_cast<const char*>(file_path.path.data());  // NOLINT
        return std::string{path_data, file_path.path.size()};
    }

    static cetl::optional<std::string> canonicalizePath(const std::string& path)
    {
        char* const resolved_path = ::realpath(path.c_str(), nullptr);
        if (resolved_path == nullptr)
        {
            return cetl::nullopt;
        }

        std::string result{resolved_path};
        ::free(resolved_path);  // NOLINT

        return result;
    }

    Svc::GetInfo::Response serveGetInfoRequest(const Svc::GetInfo::CallbackArg& arg) const
    {
        const auto request_path = stringFrom(arg.request.path);
        logger_->trace("'GetInfo' request (from={}, path='{}').", arg.metadata.remote_node_id, request_path);

        Svc::GetInfo::Response response{&memory_};
        response._error.value = uavcan::file::Error_1_0::NOT_FOUND;

        if (const auto real_path = canonicalizePath(request_path))
        {
            struct stat file_stat{};
            if (::stat(real_path->c_str(), &file_stat) == 0)
            {
                response.size                  = file_stat.st_size;
                response.is_file_not_directory = true;
                response.is_readable           = true;
                response._error.value          = uavcan::file::Error_1_0::OK;
            }
        }

        return response;
    }

    Svc::Read::Response serveReadRequest(const Svc::Read::CallbackArg& arg) const
    {
        const auto request_path = stringFrom(arg.request.path);
        const auto real_path = canonicalizePath(request_path);

        Svc::Read::Response response{&memory_};
        auto&               buffer = response.data.value;

        constexpr std::size_t BufferSize = 256U;
        buffer.resize(BufferSize);

        try
        {
            std::ifstream file{request_path.data(), std::ios::binary};
            file.seekg(static_cast<std::streamoff>(arg.request.offset));
            file.read(reinterpret_cast<char*>(buffer.data()), BufferSize);  // NOLINT
            buffer.resize(file.gcount());
            response._error.value = uavcan::file::Error_1_0::OK;

        } catch (std::exception& ex)
        {
            buffer.clear();
            logger_->error("Failed to read file '{}': {}.", request_path, ex.what());
            response._error.value = uavcan::file::Error_1_0::UNKNOWN_ERROR;
        }
        return response;
    }

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
