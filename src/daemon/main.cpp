//
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT
//

#include <array>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <signal.h>  // NOLINT *-deprecated-headers for `pid_t` type
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <unistd.h>

namespace
{

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile int s_running = 1;

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

void step_01_close_all_file_descriptors(std::array<int, 2>& pipe_fds)
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

    // Create a pipe to communicate with the original process.
    //
    if (::pipe(pipe_fds.data()) == -1)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to create pipe: " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
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

bool step_05_fork_to_background(std::array<int, 2>& pipe_fds)
{
    // Fork off the parent process
    const pid_t parent_pid = fork();
    if (parent_pid < 0)
    {
        const char* const err_txt = ::strerror(errno);
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

void step_06_create_new_session()
{
    if (::setsid() < 0)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to setsid: " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }
}

void step_07_08_fork_and_exit_again(int& pipe_write_fd)
{
    assert(pipe_write_fd != -1);

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
        ::close(pipe_write_fd);
        pipe_write_fd = -1;
        ::exit(EXIT_SUCCESS);
    }
}

void step_09_redirect_stdio_to_devnull()
{
    const int fd = ::open("/dev/null", O_RDWR);  // NOLINT *-vararg
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
    const int fd = ::open(pid_file_name, O_RDWR | O_CREAT, 0644);  // NOLINT *-vararg
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

    constexpr std::size_t             max_pid_str_len = 32;
    std::array<char, max_pid_str_len> buf{};
    const auto len = ::snprintf(buf.data(), buf.size(), "%ld\n", static_cast<long>(::getpid()));  // NOLINT *-vararg
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

void step_14_notify_init_complete(int& pipe_write_fd)
{
    assert(pipe_write_fd != -1);

    // From the daemon process, notify the original process started that initialization is complete. This can be
    // implemented via an unnamed pipe or similar communication channel that is created before the first fork() and
    // hence available in both the original and the daemon process.

    // Closing the writing end of the pipe will signal the original process that the daemon is ready.
    ::close(pipe_write_fd);
    pipe_write_fd = -1;
}

void step_15_exit_org_process(int& pipe_read_fd)
{
    // Call exit() in the original process. The process that invoked the daemon must be able to rely on that this exit()
    // happens after initialization is complete and all external communication channels are established and accessible.

    constexpr std::size_t      buf_size = 16;
    std::array<char, buf_size> buf{};
    if (::read(pipe_read_fd, buf.data(), buf.size()) > 0)
    {
        std::cout << "Child has finished initialization.\n";
    }
    ::close(pipe_read_fd);
    pipe_read_fd = -1;
    ::exit(EXIT_SUCCESS);
}

/// Implements the daemonization procedure as described in the `man 7 daemon` manual page.
///
void daemonize()
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

        step_06_create_new_session();
        step_07_08_fork_and_exit_again(pipe_fds[1]);
        step_09_redirect_stdio_to_devnull();
        step_10_reset_umask();
        step_11_change_curr_dir();
        step_12_create_pid_file("/var/run/ocvsmd.pid");
        step_13_drop_privileges();
        step_14_notify_init_complete(pipe_fds[1]);
    }
    else
    {
        // Original parent process.
        assert(pipe_fds[0] != -1);
        assert(pipe_fds[1] == -1);

        step_15_exit_org_process(pipe_fds[0]);
    }

    ::openlog("ocvsmd", LOG_PID, LOG_DAEMON);
}

}  // namespace

int main(const int argc, const char** const argv)
{
    (void) argc;
    (void) argv;

    daemonize();

    ::syslog(LOG_NOTICE, "ocvsmd daemon started.");  // NOLINT *-vararg

    while (s_running == 1)
    {
        // TODO: Insert daemon code here.
        ::sleep(1);
    }

    ::syslog(LOG_NOTICE, "ocvsmd daemon terminated.");  // NOLINT *-vararg
    ::closelog();

    return EXIT_SUCCESS;
}
