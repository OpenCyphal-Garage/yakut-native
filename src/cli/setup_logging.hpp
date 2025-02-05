//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_CLI_SETUP_LOGGING_HPP_INCLUDED
#define OCVSMD_CLI_SETUP_LOGGING_HPP_INCLUDED

#include <spdlog/cfg/argv.h>
#include <spdlog/cfg/helpers.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

namespace detail
{

inline void loadFlushLevels(const std::string& flush_levels)
{
    constexpr std::size_t max_levels_len = 512;
    if (flush_levels.empty() || (flush_levels.size() > max_levels_len))
    {
        return;
    }

    auto key_vals = spdlog::cfg::helpers::extract_key_vals_(flush_levels);  // NOLINT
    for (auto& name_level : key_vals)
    {
        const auto& logger_name = name_level.first;
        auto&       level_name  = spdlog::cfg::helpers::to_lower_(name_level.second);  // NOLINT
        const auto  level       = spdlog::level::from_str(level_name);
        // Ignore unrecognized level names.
        if (level == spdlog::level::off && level_name != "off")
        {
            continue;
        }

        if (const auto logger = spdlog::get(logger_name))
        {
            logger->flush_on(level);
        }
    }

    // Apply default flush level to all other loggers (if not specified in the `key_vals`).
    //
    const auto default_flush_level = spdlog::default_logger()->flush_level();
    spdlog::apply_all([&key_vals, default_flush_level](const auto& logger) {
        //
        // Skip default logger and loggers with explicit flush levels.
        if (!logger->name().empty() && (key_vals.find(logger->name()) == key_vals.end()))
        {
            logger->flush_on(default_flush_level);
        }
    });
}

/// Search for SPDLOG_FLUSH_LEVEL= in the args and use it to init the flush levels.
///
inline void loadArgvFlushLevels(const int argc, const char** const argv)
{
    static const std::string spdlog_level_prefix = "SPDLOG_FLUSH_LEVEL=";

    for (int i = 1; i < argc; i++)
    {
        const std::string arg_str = argv[i];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (0 == arg_str.compare(0, spdlog_level_prefix.size(), spdlog_level_prefix))
        {
            const auto levels_str = arg_str.substr(spdlog_level_prefix.size());
            loadFlushLevels(levels_str);
        }
    }
}

}  // namespace detail

/// Sets up the logging system.
///
/// File sink is used for all loggers (with Info default level).
///
inline void setupLogging(const int argc, const char** const argv)
{
    using spdlog::sinks::rotating_file_sink_st;

    try
    {
        constexpr std::size_t log_max_files     = 4;
        constexpr std::size_t log_file_max_size = 16UL * 1048576UL;  // 16 MB

        const std::string log_prefix    = "ocvsmd-cli";
        const std::string log_file_nm   = log_prefix + ".log";
        const auto        log_file_path = "./" + log_file_nm;

        // Drop all existing loggers, including the default one, so that we can reconfigure them.
        spdlog::drop_all();

        const auto file_sink = std::make_shared<rotating_file_sink_st>(  //
            log_file_path,
            log_file_max_size,
            log_max_files);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P] [%n] [%l] %v");

        const auto default_logger = std::make_shared<spdlog::logger>("", file_sink);
        register_logger(default_logger);
        set_default_logger(default_logger);

        // Register specific subsystem loggers.
        //
        register_logger(std::make_shared<spdlog::logger>("io", file_sink));
        register_logger(std::make_shared<spdlog::logger>("ipc", file_sink));
        register_logger(std::make_shared<spdlog::logger>("sdk", file_sink));
        register_logger(std::make_shared<spdlog::logger>("svc", file_sink));

        // Accept `SPDLOG_LEVEL` & `SPDLOG_FLUSH_LEVEL` arguments (like `SPDLOG_LEVEL=debug,ipc=trace`).
        //
        spdlog::cfg::load_argv_levels(argc, argv);
        detail::loadArgvFlushLevels(argc, argv);

        // Insert "--…--" just to have clearer separation in the log file between two different process runs.
        spdlog::info("--------------------------");

    } catch (const std::exception& ex)
    {
        std::cerr << "Failed to setup logging: " << ex.what() << '\n';
        std::exit(EXIT_FAILURE);
    }
}

#endif  // OCVSMD_CLI_SETUP_LOGGING_HPP_INCLUDED
