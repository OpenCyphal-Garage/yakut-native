//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#ifndef OCVSMD_DAEMON_SETUP_LOGGING_HPP_INCLUDED
#define OCVSMD_DAEMON_SETUP_LOGGING_HPP_INCLUDED

#include "config.hpp"

#include <spdlog/cfg/argv.h>
#include <spdlog/cfg/helpers.h>  // NOLINT
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <memory>
#include <string>
#include <sys/syslog.h>
#include <unistd.h>

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
    const std::string spdlog_level_prefix = "SPDLOG_FLUSH_LEVEL=";
    for (int i = 1; i < argc; i++)
    {
        const std::string arg_str = argv[i];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (arg_str.find(spdlog_level_prefix) == 0)
        {
            const auto levels_str = arg_str.substr(spdlog_level_prefix.size());
            loadFlushLevels(levels_str);
        }
    }
}

}  // namespace detail

inline bool writeString(const int fd, const char* const str)
{
    const auto str_len = strlen(str);
    return str_len == ::write(fd, str, str_len);
}

/// Sets up the logging system.
///
/// Both syslog and file logging sinks are used.
/// The syslog sink is used for the default logger only (with Info default level),
/// while the file sink is used for all loggers (with Debug default level).
///
inline void setupLogging(const int                                  err_fd,
                         const bool                                 is_daemonized,
                         const int                                  argc,
                         const char** const                         argv,
                         const ocvsmd::daemon::engine::Config::Ptr& config)
{
    using spdlog::sinks::syslog_sink_st;
    using spdlog::sinks::rotating_file_sink_st;

    try
    {
        constexpr std::size_t log_files_max     = 4;
        constexpr std::size_t log_file_max_size = 16UL * 1048576UL;  // 16 MB

        const std::string log_prefix    = "ocvsmd";
        const std::string log_file_nm   = log_prefix + ".log";
        const std::string log_file_dir  = is_daemonized ? "/var/log/" : "./";
        auto              log_file_path = log_file_dir + log_file_nm;
        if (const auto logging_file = config->getLoggingFile())
        {
            log_file_path = logging_file.value();
        }

        // Drop all existing loggers, including the default one, so that we can reconfigure them.
        spdlog::drop_all();

        const auto file_sink = std::make_shared<rotating_file_sink_st>(log_file_path, log_file_max_size, log_files_max);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%P] [%n] [%l] %v");

        const int  syslog_facility = is_daemonized ? LOG_DAEMON : LOG_USER;
        const auto syslog_sink     = std::make_shared<syslog_sink_st>(log_prefix, LOG_PID, syslog_facility, true);
        syslog_sink->set_pattern("[%l] '%n' | %v");

        // The default logger goes to all sinks.
        //
        const std::initializer_list<spdlog::sink_ptr> sinks{syslog_sink, file_sink};
        const auto                                    default_logger = std::make_shared<spdlog::logger>("", sinks);
        register_logger(default_logger);
        set_default_logger(default_logger);

        // Register specific subsystem loggers - they go to the file sink only.
        //
        register_logger(std::make_shared<spdlog::logger>("ipc", file_sink));
        register_logger(std::make_shared<spdlog::logger>("engine", file_sink));

        // Setup log levels from the configuration file.
        // Also accept `SPDLOG_LEVEL` & `SPDLOG_FLUSH_LEVEL` arguments if any (like `SPDLOG_LEVEL=debug,ipc=trace`).
        //
        if (const auto logging_level = config->getLoggingLevel())
        {
            spdlog::cfg::helpers::load_levels(logging_level.value());
        }
        if (const auto logging_flush_level = config->getLoggingFlushLevel())
        {
            detail::loadFlushLevels(logging_flush_level.value());
        }
        spdlog::cfg::load_argv_levels(argc, argv);
        detail::loadArgvFlushLevels(argc, argv);

        // Insert "--…--" just to have clearer separation in the log file between two different process runs.
        //
        if (spdlog::default_logger()->should_log(spdlog::level::info))
        {
            // It goes directly to the file sink to avoid logging into syslog.
            file_sink->log({"", spdlog::level::info, "--------------------------"});
        }

    } catch (const std::exception& ex)
    {
        writeString(err_fd, "Failed to setup logging: ");
        writeString(err_fd, ex.what());
        ::exit(EXIT_FAILURE);
    }
}

#endif  // OCVSMD_DAEMON_SETUP_LOGGING_HPP_INCLUDED
