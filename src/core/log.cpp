#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <execinfo.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "core/log.h"

namespace {

FILE *klog_open_file() {
    const char *state_home = getenv("XDG_STATE_HOME");
    std::string base = state_home && *state_home ? std::string(state_home) : std::string(getenv("HOME") ? getenv("HOME") : "") + "/.local/state";

    std::string dir;
    for (size_t pos = 1; pos <= base.size(); ++pos) {
        if (pos == base.size() || base[pos] == '/') {
            mkdir(base.substr(0, pos).c_str(), 0755);
        }
    }
    dir = base + "/astralia";
    mkdir(dir.c_str(), 0755);

    return fopen((dir + "/astralia.log").c_str(), "a");
}

FILE *&klog_file() {
    static FILE *f = klog_open_file();
    return f;
}

void klog_crash_handler(int sig) {
    void *frames[64];
    int n = backtrace(frames, 64);

    char header[64];
    int header_len = snprintf(header, sizeof(header), "astralia-shell: crashed on signal %d\n", sig);

    int fds[2] = {STDERR_FILENO, klog_file() ? fileno(klog_file()) : -1};
    for (int fd : fds) {
        if (fd < 0)
            continue;
        write(fd, header, header_len);
        backtrace_symbols_fd(frames, n, fd);
    }
    _exit(128 + sig);
}

} // namespace

void klog_install_crash_handler() {
    klog_file();
    void *warmup[8];
    backtrace(warmup, 8);

    struct sigaction sa{};
    sa.sa_handler = klog_crash_handler;
    sigemptyset(&sa.sa_mask);
    for (int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGILL, SIGFPE})
        sigaction(sig, &sa, nullptr);
}

void klog(const char *fmt, ...) {
    FILE *f = klog_file();

    timeval tv;
    gettimeofday(&tv, nullptr);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));

    FILE *outs[2] = {stderr, f};
    for (FILE *out : outs) {
        if (!out)
            continue;
        fprintf(out, "[%s.%03ld] ", timebuf, static_cast<long>(tv.tv_usec / 1000));
        va_list args;
        va_start(args, fmt);
        vfprintf(out, fmt, args);
        va_end(args);
        fputc('\n', out);
        fflush(out);
    }
}
