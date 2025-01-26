//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_ENGINE_CONFIG_HPP_INCLUDED
#define OCVSMD_DAEMON_ENGINE_CONFIG_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{

class Config
{
public:
    using Ptr = std::shared_ptr<Config>;

    using CyphalNodeId       = std::uint16_t;
    using CyphalNodeUniqueId = std::array<std::uint8_t, 16>;  // NOLINT(*-magic-numbers)

    CETL_NODISCARD static Ptr make(std::string file_path);

    Config(const Config&)                = delete;
    Config(Config&&) noexcept            = delete;
    Config& operator=(const Config&)     = delete;
    Config& operator=(Config&&) noexcept = delete;

    virtual ~Config() = default;

    virtual void save() = 0;

    CETL_NODISCARD virtual auto getCyphalNodeId() const -> cetl::optional<CyphalNodeId>             = 0;
    CETL_NODISCARD virtual auto getCyphalNodeUniqueId() const -> cetl::optional<CyphalNodeUniqueId> = 0;
    virtual void                setCyphalNodeUniqueId(const CyphalNodeUniqueId& unique_id)          = 0;
    CETL_NODISCARD virtual auto getCyphalUdpIface() const -> std::string                            = 0;

protected:
    Config() = default;

};  // Config

}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd

#endif  // OCVSMD_DAEMON_ENGINE_CONFIG_HPP_INCLUDED
