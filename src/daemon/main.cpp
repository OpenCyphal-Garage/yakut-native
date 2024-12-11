#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/resource.h>
#include <unistd.h>

namespace
{

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

void step_02_reset_all_signal_handlers_to_default()
{
    for (int sig = 1; sig < _NSIG; ++sig)
    {
        (void) ::signal(sig, SIG_DFL);
    }
}

void step_03_reset_signal_mask()
{
    sigset_t sigset_all{};
    if (::sigfillset(&sigset_all) != 0)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to sigfillset(): " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }
    if (::sigprocmask(SIG_SETMASK, &sigset_all, nullptr) != 0)
    {
        const char* const err_txt = ::strerror(errno);
        std::cerr << "Failed to sigprocmask(SIG_SETMASK): " << err_txt << "\n";
        ::exit(EXIT_FAILURE);
    }
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

/// Implements the daemonization procedure as described in the `man 7 daemon` manual page.
///
void daemonize()
{
    step_01_close_all_file_descriptors();
    step_02_reset_all_signal_handlers_to_default();
    step_03_reset_signal_mask();
    step_04_sanitize_environment();
    step_05_fork_to_background();
    step_06_create_new_session();
    step_07_08_fork_and_exit_again();
}

}  // namespace

int main(const int argc, const char** const argv)
{
    (void) argc;
    (void) argv;

    daemonize();

    return EXIT_SUCCESS;
}
