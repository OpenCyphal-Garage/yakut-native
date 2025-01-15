//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include "engine/application.hpp"

#include <spdlog/cfg/argv.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <signal.h>  // NOLINT
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace
{

const auto* const s_init_complete = "init_complete";

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile sig_atomic_t g_running = 1;

extern "C" void signalHandler(const int sig)
{
    switch (sig)
    {
    case SIGTERM:
    case SIGINT:
        g_running = 0;
        break;
    default:
        break;
    }
}

void setupSignalHandlers()
{
    struct sigaction sigbreak
    {};
    sigbreak.sa_handler = &signalHandler;
    ::sigaction(SIGINT, &sigbreak, nullptr);
    ::sigaction(SIGTERM, &sigbreak, nullptr);
}

bool writeString(const int fd, const char* const str)
{
    const auto str_len = strlen(str);
    return str_len == ::write(fd, str, str_len);
}

void exitWithFailure(const int fd, const char* const msg)
{
    const char* const err_txt = std::strerror(errno);
    writeString(fd, msg);
    writeString(fd, err_txt);
    ::exit(EXIT_FAILURE);
}

void step_01_close_all_file_descriptors(std::array<int, 2>& pipe_fds)
{
    rlimit rlimit_files{};
    if (getrlimit(RLIMIT_NOFILE, &rlimit_files) != 0)
    {
        const char* const err_txt = std::strerror(errno);
        std::cerr << "Failed to getrlimit(RLIMIT_NOFILE): " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }
    constexpr int first_fd_to_close = 3;  // 0, 1 & 2 for standard input, output, and error.
    for (int fd = first_fd_to_close; fd <= rlimit_files.rlim_cur; ++fd)
    {
        (void) ::close(fd);
    }

    // Create a pipe to communicate with the original process.
    //
    if (::pipe(pipe_fds.data()) == -1)
    {
        const char* const err_txt = std::strerror(errno);
        std::cerr << "Failed to create pipe: " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }
}

void step_02_03_setup_signal_handlers()
{
    setupSignalHandlers();
}

void step_04_sanitize_environment()
{
    // Not sure what to sanitize exactly.
}

bool step_05_fork_to_background(std::array<int, 2>& pipe_fds)
{
    // Fork off the parent process
    const pid_t parent_pid = fork();
    if (parent_pid < 0)
    {
        const char* const err_txt = std::strerror(errno);
        std::cerr << "Failed to fork: " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }

    if (parent_pid == 0)
    {
        // Close read end on the child side.
        ::close(pipe_fds[0]);
        pipe_fds[0] = -1;
    }
    else
    {
        // Close write end on the parent side.
        ::close(pipe_fds[1]);
        pipe_fds[1] = -1;
    }

    return parent_pid == 0;
}

void step_06_create_new_session(const int pipe_write_fd)
{
    if (::setsid() < 0)
    {
        exitWithFailure(pipe_write_fd, "Failed to setsid: ");
    }
}

void step_07_08_fork_and_exit_again(int& pipe_write_fd)
{
    assert(pipe_write_fd != -1);

    // Fork off the parent process
    const pid_t pid = fork();
    if (pid < 0)
    {
        exitWithFailure(pipe_write_fd, "Failed to fork: ");
    }
    if (pid > 0)
    {
        ::close(pipe_write_fd);
        pipe_write_fd = -1;
        ::exit(EXIT_SUCCESS);
    }
}

void step_09_redirect_stdio_to_devnull(const int pipe_write_fd)
{
    const int fd = ::open("/dev/null", O_RDWR);  // NOLINT *-vararg
    if (fd == -1)
    {
        exitWithFailure(pipe_write_fd, "Failed to open(/dev/null): ");
    }

    ::dup2(fd, STDIN_FILENO);
    ::dup2(fd, STDOUT_FILENO);
    ::dup2(fd, STDERR_FILENO);

    if (fd > 2)
    {
        ::close(fd);
    }
}

void step_10_reset_umask()
{
    ::umask(0);
}

void step_11_change_curr_dir(const int pipe_write_fd)
{
    if (::chdir("/") != 0)
    {
        exitWithFailure(pipe_write_fd, "Failed to chdir(/): ");
    }
}

void step_12_create_pid_file(const int pipe_write_fd)
{
    const int fd = ::open("/var/run/ocvsmd.pid", O_RDWR | O_CREAT, 0644);  // NOLINT *-vararg
    if (fd == -1)
    {
        exitWithFailure(pipe_write_fd, "Failed to create on PID file: ");
    }

    if (::lockf(fd, F_TLOCK, 0) == -1)
    {
        exitWithFailure(pipe_write_fd, "Failed to lock PID file: ");
    }

    if (::ftruncate(fd, 0) != 0)
    {
        exitWithFailure(pipe_write_fd, "Failed to ftruncate PID file: ");
    }

    constexpr std::size_t             max_pid_str_len = 32;
    std::array<char, max_pid_str_len> buf{};
    const auto len = ::snprintf(buf.data(), buf.size(), "%ld\n", static_cast<long>(::getpid()));  // NOLINT *-vararg
    if (::write(fd, buf.data(), len) != len)
    {
        exitWithFailure(pipe_write_fd, "Failed to write to PID file: ");
    }

    // Keep the PID file open until the process exits.
}

void step_13_drop_privileges()
{
    // Not sure what to drop exactly.
}

void step_14_notify_init_complete(int& pipe_write_fd)
{
    assert(pipe_write_fd != -1);

    // From the daemon process, notify the original process started that initialization is complete. This can be
    // implemented via an unnamed pipe or similar communication channel created before the first fork() and
    // hence available in both the original and the daemon process.

    // Closing the writing end of the pipe will signal the original process that the daemon is ready.
    writeString(pipe_write_fd, s_init_complete);
    ::close(pipe_write_fd);
    pipe_write_fd = -1;
}

void step_15_exit_org_process(int& pipe_read_fd)
{
    // Call exit() in the original process. The process that invoked the daemon must be able to rely on that this exit()
    // happens after initialization is complete and all external communication channels are established and accessible.

    constexpr std::size_t      buf_size = 256;
    std::array<char, buf_size> msg_from_child{};
    const auto                 res = ::read(pipe_read_fd, msg_from_child.data(), msg_from_child.size() - 1);
    if (res == -1)
    {
        const char* const err_txt = std::strerror(errno);
        std::cerr << "Failed to read pipe: " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }

    if (::strcmp(msg_from_child.data(), s_init_complete) != 0)
    {
        std::cerr << "Child init failed: " << msg_from_child.data() << "\n";
        ::exit(EXIT_FAILURE);
    }

    ::close(pipe_read_fd);
    pipe_read_fd = -1;
    ::exit(EXIT_SUCCESS);
}

/// Implements the daemonization procedure as described in the `man 7 daemon` manual page.
///
int daemonize()
{
    std::array<int, 2> pipe_fds{-1, -1};

    step_01_close_all_file_descriptors(pipe_fds);
    step_02_03_setup_signal_handlers();
    step_04_sanitize_environment();
    if (step_05_fork_to_background(pipe_fds))
    {
        // Child process.
        assert(pipe_fds[0] == -1);
        assert(pipe_fds[1] != -1);
        auto& pipe_write_fd = pipe_fds[1];

        step_06_create_new_session(pipe_write_fd);
        step_07_08_fork_and_exit_again(pipe_write_fd);
        step_09_redirect_stdio_to_devnull(pipe_write_fd);
        step_10_reset_umask();
        step_11_change_curr_dir(pipe_write_fd);
        step_12_create_pid_file(pipe_write_fd);
        step_13_drop_privileges();

        // `step_14_notify_init_complete(pipe_write_fd);` will be called by the main
        // when the application has been successfully initialized.
        return pipe_write_fd;
    }

    // Original parent process.
    assert(pipe_fds[0] != -1);
    assert(pipe_fds[1] == -1);
    auto& pipe_read_fd = pipe_fds[0];

    step_15_exit_org_process(pipe_read_fd);
    return -1;  // Unreachable actually b/c of `::exit` call.
}

/// Sets up the logging system.
///
/// Both syslog and file logging sinks are used.
/// The syslog sink is used for the default logger only (with Info default level),
/// while the file sink is used for all loggers (with Debug default level).
///
void setupLogging(const int err_fd, const bool is_daemonized, const int argc, const char** const argv)
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
        const auto        log_file_path = log_file_dir + log_file_nm;

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

        // Accept `SPDLOG_LEVEL` argument (like `SPDLOG_LEVEL=debug,ipc=trace`).
        spdlog::cfg::load_argv_levels(argc, argv);

    } catch (const std::exception& ex)
    {
        writeString(err_fd, "Failed to setup logging: ");
        writeString(err_fd, ex.what());
        ::exit(EXIT_FAILURE);
    }
}

}  // namespace

int main(const int argc, const char** const argv)
{
    using ocvsmd::daemon::engine::Application;

    bool should_daemonize = true;
    for (int i = 1; i < argc; ++i)
    {
        if (::strcmp(argv[i], "--dev") == 0)  // NOLINT
        {
            should_daemonize = false;
        }
    }

    // If daemonizing is disabled (in dev mode) then use the standard error output for reporting,
    // otherwise use the pipe to communicate with the parent process.
    int pipe_write_fd = 2;
    if (should_daemonize)
    {
        pipe_write_fd = daemonize();
        // We are in a child process now!
    }
    else
    {
        setupSignalHandlers();
    }

    setupLogging(pipe_write_fd, should_daemonize, argc, argv);

    spdlog::info("OCVSMD started (ver='{}.{}').", VERSION_MAJOR, VERSION_MINOR);
    {
        Application application;
        if (const auto failure_str = application.init())
        {
            spdlog::critical("Failed to init application: {}", failure_str.value());

            // Report the failure to the parent process (if daemonized; otherwise goes to stderr).
            writeString(pipe_write_fd, "Failed to init application: ");
            writeString(pipe_write_fd, failure_str.value().c_str());
            ::exit(EXIT_FAILURE);
        }
        if (should_daemonize)
        {
            step_14_notify_init_complete(pipe_write_fd);
        }

        application.runWhile([] { return g_running == 1; });

        if (g_running == 0)
        {
            spdlog::debug("Received termination signal.");
        }
    }
    spdlog::info("OCVSMD daemon terminated.");

    return EXIT_SUCCESS;
}
