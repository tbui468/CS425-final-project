#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "arena.h"

typedef uint16_t port_t;


#define MAX_STRING 1024

struct string {
    char *ptr;
    size_t len;
};

struct string string_dup(const struct string *s, struct arena *arena);
struct string string_from_cstring(const char *s, struct arena *arena);
char *string_to_cstring(const struct string *s, struct arena *arena);


int randint(int min, int max);

struct log {
    FILE *file;
};

enum event {
    E_PEER_JOIN = 0,
    E_PEER_FAIL,
    E_DEBUG
};

void log_init(port_t port);
void log_event(enum event event, const char *fmt, ...);

#endif //COMMON_H
