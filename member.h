#ifndef MEMBER_H
#define MEMBER_H

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
};

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


void member_list_init(struct member_list *list, struct arena *arena) {
    list->capacity = 8;
    list->count = 0;
    list->values = arena_malloc(arena, sizeof(struct member) * list->capacity);
    list->arena = arena;
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

void member_list_append(struct member_list *list, struct member *member) {
    if (list->count == list->capacity) {
        list->capacity *= 2;
        list->values = arena_realloc(list->arena, list->values, sizeof(struct member) * list->capacity);
    }


    list->values[list->count] = *member;
    list->count++;
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

size_t member_serialized_size(struct arena *arena) {
    struct member m;
    char *b;
    size_t s = member_serialize(&m, arena, &b);
    return s;
}

size_t member_list_serialize(const struct member_list *list, struct arena *arena, char **out_buf) {
    size_t member_size = member_serialized_size(arena);
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

#endif //MEMBER_H
