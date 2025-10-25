#include "common.h"


struct string string_dup(const struct string *s, struct arena *arena) {
    struct string cpy;
    cpy.len = s->len;
    cpy.ptr = arena_malloc(arena, s->len);
    memcpy(cpy.ptr, s->ptr, cpy.len);
    return cpy;    
}

struct string string_from_cstring(const char *s, struct arena *arena) {
    struct string cpy;
    cpy.len = strlen(s);
    cpy.ptr = arena_malloc(arena, cpy.len);
    memcpy(cpy.ptr, s, cpy.len);
    return cpy;    
}

char *string_to_cstring(const struct string *s, struct arena *arena) {
    char *buf = arena_malloc(arena, s->len + 1);
    memcpy(buf, s->ptr, s->len);
    buf[s->len] = '\0';
    return buf; 
}


int randint(int min, int max) {
    return (rand() % (max - min + 1)) + min;
}

static struct log g_log;
static port_t g_port;

void log_init(port_t port) {
    g_log.file = stdout;
    //g_log.file = fopen("test.log", "a");
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
        fprintf(g_log.file, "DEBUG ");
        break;
    }
    vfprintf(g_log.file, fmt, args);
    fflush(g_log.file);

    va_end(args);
}
