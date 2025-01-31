//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_ENGINE_CYPHAL_FILE_PROVIDER_HPP_INCLUDED
#define OCVSMD_DAEMON_ENGINE_CYPHAL_FILE_PROVIDER_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <libcyphal/presentation/presentation.hpp>

#include <memory>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{
namespace cyphal
{

/// @brief Defines 'File' provider component for the application node.
///
/// Internally uses several `uavcan.file` cyphal servers:
// - 'GetInfo'
// - 'List'
// - 'Modify'
// - 'Read'
// - 'Write'
///
class FileProvider
{
public:
    using Ptr = std::unique_ptr<FileProvider>;

    CETL_NODISCARD static Ptr make(cetl::pmr::memory_resource&            memory,
                                   libcyphal::presentation::Presentation& presentation);

    FileProvider(const FileProvider&)                = delete;
    FileProvider(FileProvider&&) noexcept            = delete;
    FileProvider& operator=(const FileProvider&)     = delete;
    FileProvider& operator=(FileProvider&&) noexcept = delete;

    virtual ~FileProvider() = default;

protected:
    FileProvider() = default;

};  // FileProvider

}  // namespace cyphal
}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd

#endif  // OCVSMD_DAEMON_ENGINE_CYPHAL_FILE_PROVIDER_HPP_INCLUDED
