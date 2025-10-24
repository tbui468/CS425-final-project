#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

struct log {
    FILE *file;
};

struct log g_log;
port_t g_port;

enum event {
    E_PEER_JOIN = 0,
    E_PEER_FAIL
};

void log_init(port_t port) {
    g_log.file = stdout;
    g_port = port;
}

void log_event(enum event event, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(g_log.file, "[%d]: ", g_port);
    switch (event) {
    case E_PEER_JOIN:
        fprintf(g_log.file, "PEER JOIN ");
        break;
    case E_PEER_FAIL:
        fprintf(g_log.file, "PEER FAIL ");
        break;
    default:
        assert(false);
        break;
    }
    vfprintf(g_log.file, fmt, args);
    fflush(g_log.file);

    va_end(args);
}


#endif //LOG_H
