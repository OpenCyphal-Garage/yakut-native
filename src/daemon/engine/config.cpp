//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "config.hpp"

#include <cetl/pf17/cetlpf.hpp>

#include <spdlog/spdlog.h>
#include <toml.hpp>

#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <utility>

namespace ocvsmd
{
namespace daemon
{
namespace engine
{
namespace
{

struct Default
{
    struct Cyphal
    {
        struct Udp
        {
            constexpr static auto Iface = "127.0.0.1";
        };
    };

};  // Default

class ConfigImpl final : public Config
{
public:
    using TomlConf  = toml::ordered_type_config;
    using TomlValue = toml::basic_value<TomlConf>;

    ConfigImpl(std::string file_path, TomlValue&& root)
        : file_path_{std::move(file_path)}
        , root_{std::move(root)}
        , is_dirty_{false}
    {
    }

    // Config

    void save() override
    {
        if (is_dirty_)
        {
            try
            {
                const auto    cfg_str = format(root_);
                std::ofstream file{file_path_, std::ios_base::out | std::ios_base::binary};
                file << cfg_str;

                is_dirty_ = false;

            } catch (const std::exception& ex)
            {
                spdlog::error("Failed to save config. Error: {}", file_path_, ex.what());
            }
        }
    }

    auto getCyphalNodeId() const -> cetl::optional<CyphalNodeId> override
    {
        return findImpl<CyphalNodeId>("cyphal", "node", "id");
    }

    auto getCyphalNodeUniqueId() const -> cetl::optional<CyphalNodeUniqueId> override
    {
        return findImpl<CyphalNodeUniqueId>("cyphal", "node", "unique_id");
    }

    void setCyphalNodeUniqueId(const CyphalNodeUniqueId& unique_id) override
    {
        auto& toml_unique_id = root_["cyphal"]["node"]["unique_id"];
        toml_unique_id       = unique_id;
        for (auto& item : toml_unique_id.as_array())
        {
            item.as_integer_fmt().fmt = toml::integer_format::hex;
        }
        is_dirty_ = true;
    }

    auto getCyphalUdpIface() const -> std::string override
    {
        return find_or(root_, "cyphal", "udp", "iface", Default::Cyphal::Udp::Iface);
    }

private:
    template <typename T, typename... Keys>
    cetl::optional<T> findImpl(Keys&&... keys) const
    {
        try
        {
            return toml::find<T>(root_, std::forward<Keys>(keys)...);

        } catch (...)
        {
            return cetl::nullopt;
        }
    }

    std::string file_path_;
    TomlValue   root_;
    bool        is_dirty_;

};  // ConfigImpl

}  // namespace

Config::Ptr Config::make(std::string file_path)
{
    auto maybe_root = toml::try_parse<ConfigImpl::TomlConf>(file_path);
    if (maybe_root.is_err())
    {
        const auto err_str = format_error(maybe_root.unwrap_err().at(0));
        spdlog::error("Failed to load config. Error:\n{}", err_str);
        return nullptr;
    }
    return std::make_shared<ConfigImpl>(std::move(file_path), std::move(maybe_root.unwrap()));
}

}  // namespace engine
}  // namespace daemon
}  // namespace ocvsmd