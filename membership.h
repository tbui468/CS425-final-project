#ifndef MEMBER_H
#define MEMBER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "common.h"

#define T_FAIL 16 * 4
#define T_CLEANUP T_FAIL * 2

struct arena;

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

void member_list_init(struct member_list *list, struct arena *arena, pthread_mutex_t *mtx);
void update_own_heartbeat(struct member_list *list, port_t port, int global_timestamp, int global_heartbeat);
size_t serialize_nonfailed_members(struct member_list *list, struct arena *arena, char **out_buf, int global_timestamp);
bool member_list_random_entry(struct member_list *list, struct member *result, port_t my_port);
void member_list_cleanup(struct member_list *list, int global_timestamp);
void member_list_update(struct member_list *list, struct member *member, int global_timestamp);
void member_list_merge(struct member_list *list, struct member_list *other, int global_timestamp);
void member_list_deserialize(char *buf, struct member_list *list);

#endif //MEMBER_H
