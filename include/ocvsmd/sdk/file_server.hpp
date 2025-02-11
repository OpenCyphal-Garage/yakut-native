//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_SDK_FILE_SERVER_HPP_INCLUDED
#define OCVSMD_SDK_FILE_SERVER_HPP_INCLUDED

#include "execution.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace ocvsmd
{
namespace sdk
{

class FileServer
{
public:
    using Ptr = std::shared_ptr<FileServer>;

    FileServer(FileServer&&)                 = delete;
    FileServer(const FileServer&)            = delete;
    FileServer& operator=(FileServer&&)      = delete;
    FileServer& operator=(const FileServer&) = delete;

    virtual ~FileServer() = default;

    struct ListRoots final
    {
        using Success = std::vector<std::string>;
        using Failure = int;  // `errno`-like error code.
        using Result  = cetl::variant<Success, Failure>;
    };
    virtual SenderOf<ListRoots::Result>::Ptr listRoots() = 0;

    struct PopRoot final
    {
        using Success = cetl::monostate;
        using Failure = int;  // `errno`-like error code.
        using Result  = cetl::variant<Success, Failure>;
    };
    virtual SenderOf<PopRoot::Result>::Ptr popRoot(const std::string& path, const bool back) = 0;

    struct PushRoot final
    {
        using Success = cetl::monostate;
        using Failure = int;  // `errno`-like error code.
        using Result  = cetl::variant<Success, Failure>;
    };
    virtual SenderOf<PushRoot::Result>::Ptr pushRoot(const std::string& path, const bool back) = 0;

protected:
    FileServer() = default;

};  // FileServer

}  // namespace sdk
}  // namespace ocvsmd

#endif  // OCVSMD_SDK_FILE_SERVER_HPP_INCLUDED
