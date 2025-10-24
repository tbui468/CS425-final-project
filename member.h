#ifndef MEMBER_H
#define MEMBER_H

#include "log.h"
#include "util.h"

#define T_FAIL 16 * 1
#define T_CLEANUP T_FAIL * 2

struct member {
    port_t port;
    uint64_t heartbeat;
    uint64_t timestamp;
};

struct member_list {
    struct member *values;
    int capacity;
    int count;
    struct arena *arena;
    pthread_mutex_t *mtx;
};

bool member_failed(struct member *member, int global_time) {
    return global_time - member->timestamp >= T_FAIL;
}


bool member_cleanup(struct member *member, int global_time) {
    return global_time - member->timestamp >= T_CLEANUP;
}

void memberlist_lock(struct member_list *list) {
    if (list->mtx) pthread_mutex_lock(list->mtx);
}

void memberlist_unlock(struct member_list *list) {
    if (list->mtx) pthread_mutex_unlock(list->mtx);
}

void member_to_cstring(struct member *member, char *buf, size_t maxsize) {
    snprintf(buf, maxsize, "<port: %d> <heartbeat: %ld> <timestamp: %ld>\n", member->port, member->heartbeat, member->timestamp);
}
void member_print(struct member *member) {
    printf("port: %d, heartbeat: %ld, timestamp: %ld\n", member->port, member->heartbeat, member->timestamp);
}

void member_list_print(struct member_list *list) {
    for (int i = 0; i < list->count; i++) {
        member_print(&list->values[i]);
    }
}


void member_list_init(struct member_list *list, struct arena *arena, pthread_mutex_t *mtx) {
    list->capacity = 8;
    list->count = 0;
    list->values = arena_malloc(arena, sizeof(struct member) * list->capacity);
    list->arena = arena;
    list->mtx = mtx;
}

int member_list_idx(struct member_list *list, port_t port) {
    for (int i = 0; i < list->count; i++) {
        struct member *cur = &list->values[i];
        if (cur->port == port) {
            return i;
        }
    }
    
    return -1;
}

void update_own_heartbeat(struct member_list *list, port_t port, int global_timestamp, int global_heartbeat) {
    int idx;
    assert((idx = member_list_idx(list, port)) >= 0);
    list->values[idx].heartbeat = global_heartbeat;
    list->values[idx].timestamp = global_timestamp;
}

void member_list_append(struct member_list *list, struct member *member) {
    memberlist_lock(list);

    if (list->count == list->capacity) {
        list->capacity *= 2;
        list->values = arena_realloc(list->arena, list->values, sizeof(struct member) * list->capacity);
    }


    list->values[list->count] = *member;
    list->count++;

    memberlist_unlock(list);
}


size_t member_serialize(const struct member *member, struct arena *arena, char **out_buf) {
    size_t size = sizeof(struct member);
    char *buf = arena_malloc(arena, size);
    memcpy(buf, member, sizeof(struct member));
    
    *out_buf = buf;
    return size;
}

size_t member_deserialize(char *buf, struct member *member) {
    memcpy(member, buf, sizeof(struct member));
    return sizeof(struct member);
}

size_t member_list_serialize(const struct member_list *list, struct arena *arena, char **out_buf) {
    size_t member_size = sizeof(struct member);
    size_t list_size = sizeof(uint32_t) + list->count * member_size;
    char *buf = arena_malloc(arena, list_size);
    uint32_t fixed_count = list->count;
    memcpy(buf, &fixed_count, sizeof(uint32_t));
    char *b;
    for (int i = 0; i < list->count; i++) {
        struct member *m = &list->values[i]; 
        assert(member_serialize(m, arena, &b) == member_size);
        memcpy(buf + sizeof(uint32_t) + i * member_size, b, member_size);
    }

    *out_buf = buf;
    return list_size;
}

void member_list_deserialize(char *buf, struct member_list *list) {
    list->count = 0;
    uint32_t count = *((uint32_t*) buf);
    buf += sizeof(uint32_t);

    for (int i = 0; i < count; i++) {
        struct member member;
        buf += member_deserialize(buf, &member);
        member_list_append(list, &member);
    }
}


void member_list_cleanup(struct member_list *list, int global_timestamp) {
    int idx = 0;
    while (idx < list->count) {
        struct member *cur = &list->values[idx];
        if (member_cleanup(cur, global_timestamp)) {
            char buf[MAX_STRING];
            member_to_cstring(cur, buf, MAX_STRING);
            log_event(E_PEER_FAIL, "%s", buf);

            memcpy(cur, &list->values[list->count - 1], sizeof(struct member));
            list->count--;
        } else {
            idx++;        
        }
    }
}

void member_list_update(struct member_list *list, struct member *member, int global_timestamp) {
    int idx;
    if ((idx = member_list_idx(list, member->port)) >= 0) {
        struct member *cur = &list->values[idx];
        if (member->heartbeat > cur->heartbeat && !member_failed(cur, global_timestamp)) {
            cur->heartbeat = member->heartbeat;
            cur->timestamp = global_timestamp;
        }
    } else {
        member->timestamp = global_timestamp;
        member_list_append(list, member);

        char buf[MAX_STRING];
        member_to_cstring(&list->values[list->count - 1], buf, MAX_STRING);
        log_event(E_PEER_JOIN, "%s", buf);
    }
}

bool member_list_random_entry(struct member_list *list, struct member *result, port_t my_port) {
    assert(list->count > 0);
    if (list->count == 1) {
        return false; 
    }

    while (true) {
        int r = randint(0, list->count - 1);
        struct member *cur = &list->values[r];
        if (cur->port != my_port) {
            *result = *cur;
            return true;
        }
    }

    assert(false);
    return true;  
}

void member_list_merge(struct member_list *list, struct member_list *other, int global_timestamp) {
    for (int i = 0; i < other->count; i++) {
        struct member cur = other->values[i];
        member_list_update(list, &cur, global_timestamp);
    }
}

int member_list_failed_members(const struct member_list *list, int global_timestamp) {
    int count = 0;
    for (int i = 0; i < list->count; i++) {
        struct member *m = &list->values[i]; 
        if (member_failed(m, global_timestamp)) {
            count++;
        }
    }

    return count;
}

size_t serialize_nonfailed_members(const struct member_list *list, struct arena *arena, char **out_buf, int global_timestamp) {
    struct member_list nonfailed;
    member_list_init(&nonfailed, arena, NULL);
    for (int i = 0; i < list->count; i++) {
        struct member *m = &list->values[i]; 
        if (!member_failed(m, global_timestamp)) {
            member_list_append(&nonfailed, m);
        }
    }

    return member_list_serialize(&nonfailed, arena, out_buf);
}

/*
    Just for testing
*/
/*
void member_serialization_test(struct arena *arena) {
    struct member m = { .port=get_introducer() };
    char *b;
    size_t l = member_serialize(&m, arena, &b);
    struct member n;
    assert(member_deserialize(b, &n));
    printf("expect 3000: %d\n", n.port);
}


void member_list_serialization_test(struct arena *arena) {
    struct member_list my_list;
    member_list_init(&my_list, arena, NULL);
    //make a bunch of fake members
    struct member m0 = { .port=3000 };
    struct member m1 = { .port=3001 };
    struct member m2 = { .port=3002 };
    struct member m3 = { .port=3003 };
    struct member m4 = { .port=3004 };
    struct member m5 = { .port=3005 };
    struct member m6 = { .port=3006 };
    struct member m7 = { .port=3007 };
    struct member m8 = { .port=3008 };
    member_list_update(&my_list, &m0, g_timestamp);
    member_list_update(&my_list, &m1, g_timestamp);
    member_list_update(&my_list, &m2, g_timestamp);
    member_list_update(&my_list, &m3, g_timestamp);
    member_list_update(&my_list, &m4, g_timestamp);
    member_list_update(&my_list, &m5, g_timestamp);
    member_list_update(&my_list, &m6, g_timestamp);
    member_list_update(&my_list, &m7, g_timestamp);
    member_list_update(&my_list, &m8, g_timestamp);
    member_list_print(&my_list);

    //serialize list
    char *b;
    size_t size = member_list_serialize(&my_list, arena, &b);
    printf("my_list serialized size: %ld\n", size);

    //deserialize into other list
    struct member_list other_list;
    member_list_init(&other_list, arena, NULL);
    member_list_deserialize(b, &other_list);
    member_list_print(&other_list);

    //check that members are what we expect
}
*/

#endif //MEMBER_H
