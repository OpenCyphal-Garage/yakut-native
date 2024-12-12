//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <unistd.h>

namespace
{

volatile int s_running = 1;  // NOLINT

extern "C" void handle_signal(const int sig)
{
    switch (sig)
    {
    case SIGTERM:
    case SIGINT:
        s_running = 0;
        break;
    default:
        break;
    }
}

void fork_and_exit_parent()
{
    // Fork off the parent process
    const pid_t pid = fork();
    if (pid < 0)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to fork: " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }
    if (pid > 0)
    {
        ::exit(EXIT_SUCCESS);
    }
}

void step_01_close_all_file_descriptors()
{
    rlimit rlimit_files{};
    if (getrlimit(RLIMIT_NOFILE, &rlimit_files) != 0)
    {
        const char* const err_txt = strerror(errno);
        std::cerr << "Failed to getrlimit(RLIMIT_NOFILE): " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }
    constexpr int first_fd_to_close = 3;  // 0, 1 & 2 for standard input, output, and error.
    for (int fd = first_fd_to_close; fd <= rlimit_files.rlim_max; ++fd)
    {
        (void) ::close(fd);
    }
}

void step_02_03_setup_signal_handlers()
{
    // Catch termination signals
    (void) ::signal(SIGTERM, handle_signal);
    (void) ::signal(SIGINT, handle_signal);
}

void step_04_sanitize_environment()
{
    // TODO: Implement this step.
}

void step_05_fork_to_background()
{
    fork_and_exit_parent();
}

void step_06_create_new_session()
{
    if (::setsid() < 0)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to setsid: " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }
}

void step_07_08_fork_and_exit_again()
{
    fork_and_exit_parent();
}

void step_09_redirect_stdio_to_devnull()
{
    const int fd = ::open("/dev/null", O_RDWR);  // NOLINT
    if (fd == -1)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to open(/dev/null): " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
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

void step_11_change_curr_dir()
{
    if (::chdir("/") != 0)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to chdir(/): " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }
}

void step_12_create_pid_file(const char* const pid_file_name)
{
    const int fd = ::open(pid_file_name, O_RDWR | O_CREAT, 0644);  // NOLINT
    if (fd == -1)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to create on PID file: " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }

    if (::lockf(fd, F_TLOCK, 0) == -1)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to lock PID file: " << err_txt << "\n";
        ::close(fd);
        ::exit(EXIT_FAILURE);
    }

    if (::ftruncate(fd, 0) != 0)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to ftruncate PID file: " << err_txt << "\n";
        ::close(fd);
        ::exit(EXIT_FAILURE);
    }

    std::array<char, 32> buf{};
    const auto           len = ::snprintf(buf.data(), buf.size(), "%ld\n", static_cast<long>(::getpid()));  // NOLINT
    if (::write(fd, buf.data(), len) != len)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to write to PID file: " << err_txt << "\n";
        ::close(fd);
        ::exit(EXIT_FAILURE);
    }

    // Keep the PID file open until the process exits.
}

void step_13_drop_privileges()
{
    // n the daemon process, drop privileges, if possible and applicable.
    // TODO: Implement this step.
}

void step_14_notify_init_complete()
{
    // From the daemon process, notify the original process started that initialization is complete. This can be
    // implemented via an unnamed pipe or similar communication channel that is created before the first fork() and
    // hence available in both the original and the daemon process.
    // TODO: Implement this step.
}

void step_15_exit_org_process()
{
    // Call exit() in the original process. The process that invoked the daemon must be able to rely on that this exit()
    // happens after initialization is complete and all external communication channels are established and accessible.
    // TODO: Implement this step.
}

/// Implements the daemonization procedure as described in the `man 7 daemon` manual page.
///
void daemonize()
{
    step_01_close_all_file_descriptors();
    step_02_03_setup_signal_handlers();
    step_04_sanitize_environment();
    step_05_fork_to_background();
    step_06_create_new_session();
    step_07_08_fork_and_exit_again();
    step_09_redirect_stdio_to_devnull();
    step_10_reset_umask();
    step_11_change_curr_dir();
    step_12_create_pid_file("/var/run/ocvsmd.pid");
    step_13_drop_privileges();
    step_14_notify_init_complete();
    step_15_exit_org_process();

    ::openlog("ocvsmd", LOG_PID, LOG_DAEMON);
}

}  // namespace

int main(const int argc, const char** const argv)
{
    (void) argc;
    (void) argv;

    daemonize();

    ::syslog(LOG_NOTICE, "ocvsmd daemon started.");  // NOLINT

    while (s_running == 1)
    {
        // TODO: Insert daemon code here.
        ::sleep(1);
    }

    ::syslog(LOG_NOTICE, "ocvsmd daemon terminated.");  // NOLINT
    ::closelog();

    return EXIT_SUCCESS;
}
