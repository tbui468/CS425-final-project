#ifndef ARENA_H
#define ARENA_H

#include <assert.h>
#define DEFAULT_CHUNK_SIZE 4096

union align {
    int i;
    long l;
    long *lp;
    void *p;
    void (*fp)(void);
    float f;
    double d;
    long double ld;
};

static size_t aligned_size(size_t size) {
    return ((size + sizeof(union align) - 1) / sizeof(union align)) * sizeof(union align);
}

struct chunk {
    void *base;
    size_t off;
    size_t size;
};

static void chunk_init(struct chunk *chunk, void* (*malloc)(size_t), size_t size) {
    chunk->base = malloc(size);
    if (!chunk->base) {
        exit(1);
    }
    chunk->off = 0;
    chunk->size = size;
}

static void *chunk_alloc(struct chunk *chunk, size_t size) {
    //round up to strictest alignment requirements
    size_t alsize = aligned_size(size);

    if (chunk->off + alsize > chunk->size) {
        return NULL;
    }

    void *ptr = (char*) chunk->base + chunk->off;
    chunk->off += alsize;
    return ptr;
}

struct arena {
    struct chunk *chunks;
    int chunk_count;
    int chunk_cap;
    int idx;
    void* (*malloc)(size_t);
    void* (*realloc)(void*, size_t);
    void (*free)(void*);
};

static void arena_init(struct arena *arena, 
                       void* (*malloc)(size_t), 
                       void* (*realloc)(void*, size_t), 
                       void (*free)(void*)) {
    arena->malloc = malloc;
    arena->realloc = realloc;
    arena->free = free;
    arena->chunk_cap = 8;
    arena->chunks = arena->malloc(sizeof(struct chunk) * arena->chunk_cap);
    if (!arena->chunks) {
        exit(1);
    }

    arena->chunk_count = 1;
    chunk_init(&arena->chunks[0], arena->malloc, DEFAULT_CHUNK_SIZE);
    arena->idx = 0;
}

static void arena_cleanup(struct arena *arena) {
    for (int i = 0; i < arena->chunk_count; i++) {
        struct chunk *c = &arena->chunks[i];
        arena->free(c->base);
    }
    arena->free(arena->chunks);
}

static void arena_dealloc_all(struct arena *arena) {
    for (int i = 0; i < arena->chunk_count; i++) {
        struct chunk *c = &arena->chunks[i];
        c->off = 0;
    }
    arena->idx = 0;
}

static void arena_append_chunk(struct arena *arena, const struct chunk *chunk) {
    if (arena->chunk_count >= arena->chunk_cap) {
        arena->chunk_cap *= 2;
        arena->chunks = arena->realloc(arena->chunks, sizeof(struct chunk) * arena->chunk_cap); 
        if (!arena->chunks) {
            exit(1);
        }
    }

    arena->chunks[arena->chunk_count] = *chunk;
    arena->chunk_count++;
}

static void *arena_malloc(struct arena *arena, size_t size) {
    //round up to strictest alignment requirements
    size_t alsize = aligned_size(size);

    void *ptr = NULL;
    while (arena->idx < arena->chunk_count) {
        struct chunk *cur = &arena->chunks[arena->idx];
        ptr = chunk_alloc(cur, alsize);
        if (!ptr) {
            arena->idx++;
        } else {
            return ptr;
        }
    }

    if (!ptr) {
        size_t new_size = DEFAULT_CHUNK_SIZE;
        while (new_size < alsize) {
            new_size *= 8;
        }
        struct chunk new_chunk;
        chunk_init(&new_chunk, arena->malloc, new_size);
        arena_append_chunk(arena, &new_chunk);
        ptr = chunk_alloc(&arena->chunks[arena->chunk_count - 1], alsize);
        assert(ptr);
    }

    return ptr;
}
static void *arena_calloc(struct arena *arena, size_t nmem, size_t size)  {
    //round up to strictest alignment requirements
    size_t alsize = aligned_size(nmem * size);

    void *ptr = arena_malloc(arena, alsize);
    memset(ptr, 0, alsize);
    return ptr;
}
static void *arena_realloc(struct arena *arena, void *ptr, size_t size)  {
    //round up to strictest alignment requirements
    size_t alsize = aligned_size(size);

    size_t copy_len = 0;
    bool found = false; //used to test assert
    if (ptr) {
        for (int i = 0; i < arena->chunk_count; i++) {
            struct chunk *c = &arena->chunks[i];
            //found chunk of first allocation
            if (ptr >= c->base && ptr < (void*) ((char*) c->base + c->off)) {
                size_t ptr_off = (char*) ptr - (char*) c->base;
                assert(c->off > ptr_off);
                copy_len = c->off - ptr_off;
                copy_len = copy_len > alsize ? alsize : copy_len; //copy length will be smaller size
                //copy_len = alsize > copy_len ? copy_len : alsize; //copy length will be smaller size
                found = true;
                break;
            }
        }
    }


    if (!(!ptr || found)) {
        exit(1);
    }

    void *new_ptr = arena_malloc(arena, alsize);
    if (ptr) {
        memcpy(new_ptr, ptr, copy_len);
    }

    return new_ptr;
}

char *arena_strdup(struct arena *arena, const char *s) {
    size_t len = strlen(s);
    char *p = arena_malloc(arena, len + 1);
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

#endif //ARENA_H
