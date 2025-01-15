//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_COMMON_LOGGING_HPP_INCLUDED
#define OCVSMD_COMMON_LOGGING_HPP_INCLUDED

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>

namespace ocvsmd
{
namespace common
{

using Logger    = spdlog::logger;
using LoggerPtr = std::shared_ptr<Logger>;

inline LoggerPtr getLogger(const std::string& name)
{
    if (auto logger = spdlog::get(name))
    {
        return logger;
    }

    auto default_logger = spdlog::default_logger();
    CETL_DEBUG_ASSERT(default_logger, "default");

    auto logger = default_logger->clone(name);
    CETL_DEBUG_ASSERT(logger, name.c_str());

    apply_logger_env_levels(logger);
    register_logger(logger);

    return logger;
}

}  // namespace common
}  // namespace ocvsmd

#endif  // OCVSMD_COMMON_LOGGING_HPP_INCLUDED
