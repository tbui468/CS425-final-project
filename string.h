#ifndef STRING_H
#define STRING_H

#include "arena.h"

#define MAX_STRING 1024

struct string {
    char *ptr;
    size_t len;
};

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


#endif //STRING_H
