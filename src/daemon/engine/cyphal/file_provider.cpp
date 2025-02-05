//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "config.hpp"
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

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <ios>
#include <memory>
#include <stdlib.h>  // NOLINT ::realpath
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

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
    static Ptr make(cetl::pmr::memory_resource&            memory,
                    libcyphal::presentation::Presentation& presentation,
                    Config::Ptr                            config)
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
                                                  std::move(config),
                                                  std::move(*list_srv),
                                                  std::move(*read_srv),
                                                  std::move(*write_srv),
                                                  std::move(*modify_srv),
                                                  std::move(*get_info_srv));
    }

    FileProviderImpl(cetl::pmr::memory_resource& memory,
                     Config::Ptr                 config,
                     Svc::List::Server&&         list_srv,
                     Svc::Read::Server&&         read_srv,
                     Svc::Write::Server&&        write_srv,
                     Svc::Modify::Server&&       modify_srv,
                     Svc::GetInfo::Server&&      get_info_srv)
        : memory_{memory}
        , config_{std::move(config)}
        , list_srv_{std::move(list_srv)}
        , read_srv_{std::move(read_srv)}
        , write_srv_{std::move(write_srv)}
        , modify_srv_{std::move(modify_srv)}
        , get_info_srv_{std::move(get_info_srv)}
    {
        logger_->trace("FileProviderImpl().");

        roots_ = config_->getFileServerRoots();
        logger_->debug("There are {} file server roots.", roots_.size());
        for (std::size_t i = 0; i < roots_.size(); ++i)
        {
            if (const auto real_root = canonicalizePath(roots_[i]))
            {
                logger_->trace("{:4} '{}' → '{}'", i, roots_[i], *real_root);
            }
            else
            {
                logger_->warn("{:4} '{}' → ❌ not found!", i, roots_[i]);
            }
        }

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
    static auto makeServer(const cetl::string_view role,
                           Presentation&           presentation) -> cetl::optional<typename Service::Server>
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

    static auto convertErrorCode(const int code) -> uavcan::file::Error_1_0::_traits_::TypeOf::value
    {
        switch (code)
        {
        case EIO:
            return uavcan::file::Error_1_0::IO_ERROR;
        case ENOENT:
            return uavcan::file::Error_1_0::NOT_FOUND;
        case EISDIR:
            return uavcan::file::Error_1_0::IS_DIRECTORY;
        case ENOSPC:
            return uavcan::file::Error_1_0::OUT_OF_SPACE;
        case EACCES:
            return uavcan::file::Error_1_0::ACCESS_DENIED;
        case EINVAL:
            return uavcan::file::Error_1_0::INVALID_VALUE;
        case ENOTSUP:
            return uavcan::file::Error_1_0::NOT_SUPPORTED;
        case E2BIG:
            return uavcan::file::Error_1_0::FILE_TOO_LARGE;
        default:
            return uavcan::file::Error_1_0::UNKNOWN_ERROR;
        }
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

    static cetl::optional<std::string> buildAndValidateRootWithPath(const std::string& root, const std::string& file)
    {
        if (const auto root_path = canonicalizePath(root))
        {
            if (auto sure_file_path = canonicalizePath(root + "/" + file))
            {
                auto& file_path = *sure_file_path;

                // This is a security check to ensure that the resolved path is UNDER the real root path.
                // The last `/` check is important to ensure that the `root_len` path is a directory.
                //
                const auto root_len = root_path->size();
                if ((0 == file_path.compare(0, root_len, *root_path)) && (file_path[root_len] == '/'))
                {
                    return sure_file_path;
                }
            }
        }
        return cetl::nullopt;
    }

    cetl::optional<std::pair<std::string, struct stat>> findFirstValidFile(const std::string& request_path) const
    {
        for (const auto& root : roots_)
        {
            if (const auto real_path = buildAndValidateRootWithPath(root, request_path))
            {
                // As "best effort" strategy, we skip anything we can't even `stat`.
                //
                struct stat file_stat
                {};
                if (::stat(real_path->c_str(), &file_stat) == 0)
                {
                    return std::make_pair(*real_path, file_stat);
                }
            }
        }
        return cetl::nullopt;
    }

    Svc::GetInfo::Response serveGetInfoRequest(const Svc::GetInfo::CallbackArg& arg) const
    {
        const auto request_path = stringFrom(arg.request.path);
        logger_->trace("'GetInfo' request (from={}, path='{}').", arg.metadata.remote_node_id, request_path);

        Svc::GetInfo::Response response{&memory_};
        if (const auto path_and_stat = findFirstValidFile(request_path))
        {
            const auto& file_path = path_and_stat->first;
            logger_->debug("Found file info (request='{}', real_path='{}').", request_path, file_path);
            const auto& stat = path_and_stat->second;

            response._error.value                        = uavcan::file::Error_1_0::OK;
            response.size                                = stat.st_size;
            response.unix_timestamp_of_last_modification = stat.st_mtime;
            response.is_file_not_directory               = !S_ISDIR(stat.st_mode);
            response.is_link                             = false;  // `::realpath()` resolved all possible links
            response.is_readable                         = (0 == ::access(file_path.c_str(), R_OK));
            response.is_writeable                        = (0 == ::access(file_path.c_str(), W_OK));
            return response;
        }

        response._error.value = uavcan::file::Error_1_0::NOT_FOUND;
        return response;
    }

    Svc::Read::Response serveReadRequest(const Svc::Read::CallbackArg& arg) const
    {
        using DataType             = Svc::Read::Response::_traits_::TypeOf::data;
        constexpr auto MaxDataSize = DataType::_traits_::ArrayCapacity::value;

        Svc::Read::Response response{&memory_};

        // Find the first valid file candidate in the list of roots.
        //
        const auto path_and_stat = findFirstValidFile(stringFrom(arg.request.path));
        if (!path_and_stat)
        {
            response._error.value = uavcan::file::Error_1_0::NOT_FOUND;
            return response;
        }

        // Don't allow reading beyond the end of the file.
        //
        const auto& file_path = path_and_stat->first;
        if (arg.request.offset >= path_and_stat->second.st_size)
        {
            response._error.value = uavcan::file::Error_1_0::OK;
            return response;
        }

        auto&      buffer        = response.data.value;
        const auto bytes_to_read = std::min<std::size_t>(path_and_stat->second.st_size - arg.request.offset, MaxDataSize);
        buffer.resize(bytes_to_read);
        try
        {
            std::ifstream file{file_path.c_str(), std::ios::binary};
            file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
            file.seekg(static_cast<std::streamoff>(arg.request.offset));
            file.read(reinterpret_cast<char*>(buffer.data()), bytes_to_read);  // NOLINT
            buffer.resize(file.gcount());
            response._error.value = uavcan::file::Error_1_0::OK;

        } catch (const std::ios_base::failure& ex)
        {
            buffer.clear();
            logger_->error("Failed to read file '{}' (err={}): {}.", file_path, ex.code().value(), ex.what());
            response._error.value = convertErrorCode(ex.code().value());
        }
        return response;
    }

    cetl::pmr::memory_resource& memory_;
    Config::Ptr                 config_;
    Svc::List::Server           list_srv_;
    Svc::Read::Server           read_srv_;
    Svc::Write::Server          write_srv_;
    Svc::Modify::Server         modify_srv_;
    Svc::GetInfo::Server        get_info_srv_;
    common::LoggerPtr           logger_{common::getLogger("engine")};
    std::vector<std::string>    roots_;

};  // FileProviderImpl

}  // namespace

FileProvider::Ptr FileProvider::make(cetl::pmr::memory_resource&            memory,
                                     libcyphal::presentation::Presentation& presentation,
                                     Config::Ptr                            config)
{
    return FileProviderImpl::make(memory, presentation, std::move(config));
}

}  // namespace cyphal
}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd
