#include "net.h"
#include "msg.h"
#include <stdio.h>


struct msg_queue *msg_queue;
struct arena *g_arena;


static inline bool is_introducer(struct port *port) {
    return memcmp(port->ptr, "3000", 4) == 0;
}

static inline struct port get_introducer() {
    return (struct port) { .ptr="3000", .len=4 };
}

struct member {
    struct port port;
};

struct member_list {
    struct member *values;
    int capacity;
    int count;
};

struct member_list *g_member_list;

void member_list_init(struct member_list *list) {
    list->capacity = 8;
    list->count = 0;
    list->values = arena_malloc(g_arena, sizeof(struct member) * list->capacity);
}

void member_list_append(struct member_list *list, struct member *member) {
    if (list->count == list->capacity) {
        list->capacity *= 2;
        list->values = arena_realloc(g_arena, list->values, sizeof(struct member) * list->capacity);
    }

    list->values[list->count] = *member;
    list->count++;
}

void member_print(struct member *member) {
    printf("port: %.*s\n", (int) member->port.len, member->port.ptr);
}

void member_list_print(struct member_list *list) {
    for (int i = 0; i < list->count; i++) {
        member_print(&list->values[i]);
    }
}

size_t member_serialize(const struct member *member, struct arena *arena, char **out_buf) {
    size_t size = sizeof(uint8_t) + sizeof(member->port.ptr);
    char *buf = arena_malloc(arena, size);
    uint8_t len = member->port.len;
    memcpy(buf, &len, sizeof(uint8_t));
    memcpy(buf + sizeof(uint8_t), member->port.ptr, sizeof(member->port.ptr));
    
    *out_buf = buf;
    return size;
}

size_t member_deserialize(char *buf, struct member *member) {
    uint8_t len = *((uint8_t*) buf);
    member->port.len = len;
    memcpy(member->port.ptr, buf + sizeof(uint8_t), member->port.len);
    return sizeof(uint8_t) + sizeof(member->port.ptr);
}

size_t member_serialized_size(struct arena *arena) {
    struct member m = { .port=get_introducer() };
    char *b;
    size_t s = member_serialize(&m, arena, &b);
    return s;
}

//uint32_t count
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

    printf("deserialized count: %d\n", count);

    for (int i = 0; i < count; i++) {
        struct member member;
        buf += member_deserialize(buf, &member);
        member_list_append(list, &member);
    }
}

void *node_listen(void *arg) {
    int serverfd = net_listen(arg);
    struct arena arena;
    arena_init(&arena, malloc, realloc, free);

    while (true) {
        struct msg msg;
        if (node_recv_msg(serverfd, &arena, &msg)) {
            //print_msg(&msg);
            msg_queue_enqueue(msg_queue, &msg);
        }
        arena_dealloc_all(&arena);
    }
}

void handle_msg(struct msg *msg) {
    switch (msg->type) {
    case MT_GREP_REQ: {
        print_msg(msg);

        char *cstring = string_to_cstring(&msg->payload, &msg_queue->arena);

        FILE * cmd = popen(cstring, "r");
        if (cmd == NULL) {
            exit(1);
        }

        //read local grep results
        size_t capacity = 32;
        char *result = arena_malloc(&msg_queue->arena, capacity);
        result[0] = '\0';
        size_t size = strlen(result);
        while (fgets(result + size, capacity - size, cmd)) {
            size = strlen(result);
            if (size == capacity - 1) {
                capacity *= 2;
                result = arena_realloc(&msg_queue->arena, result, capacity);
            }
        }

        pclose(cmd); 

        //construct and send response
        struct msg res_msg;
        res_msg.type = MT_GREP_RES;
        res_msg.src = msg->dst;
        res_msg.dst = msg->src;
        res_msg.payload = (struct string) { .ptr = result, .len = size };
        if (!node_send_msg(&res_msg, &msg_queue->arena)) {
            printf("failed to connect\n");
        } else {
            printf("sent response\n");
        }
        break;
    }
    case MT_JOIN_REQ: {
        struct member member = { .port=msg->src };
        member_list_append(g_member_list, &member);
        member_list_print(g_member_list);
        char *buf;
        size_t size = member_list_serialize(g_member_list, &msg_queue->arena, &buf);
        struct msg res_msg = { .type=MT_JOIN_RES, .src=msg->dst, .dst=msg->src, .payload=(struct string) { .ptr=buf, .len=size } };
        if (!node_send_msg(&res_msg, &msg_queue->arena)) {
            printf("failed to connect\n");
        } else {
            printf("sent response\n");
        }
        break;
    }

    case MT_JOIN_RES: {
        member_list_deserialize(msg->payload.ptr, g_member_list);
        member_list_print(g_member_list);
        break;
    }
    default:
        printf("message handler not implemented\n");
        break;
    }
}

void member_serialization_test(struct arena *arena) {
    struct member m = { .port=get_introducer() };
    char *b;
    size_t l = member_serialize(&m, arena, &b);
    struct member n;
    assert(member_deserialize(b, &n));
    printf("expect 3000: %.*s\n", (int) n.port.len, n.port.ptr);
}


void member_list_serialization_test(struct arena *arena) {
    struct member_list my_list;
    member_list_init(&my_list);
    //make a bunch of fake members
    struct member m0 = { .port=(struct port) { .ptr="3000", .len=4 } };
    struct member m1 = { .port=(struct port) { .ptr="3001", .len=4 } };
    struct member m2 = { .port=(struct port) { .ptr="3002", .len=4 } };
    struct member m3 = { .port=(struct port) { .ptr="3003", .len=4 } };
    struct member m4 = { .port=(struct port) { .ptr="3004", .len=4 } };
    struct member m5 = { .port=(struct port) { .ptr="3005", .len=4 } };
    struct member m6 = { .port=(struct port) { .ptr="3006", .len=4 } };
    struct member m7 = { .port=(struct port) { .ptr="3007", .len=4 } };
    struct member m8 = { .port=(struct port) { .ptr="3008", .len=4 } };
    member_list_append(&my_list, &m0);
    member_list_append(&my_list, &m1);
    member_list_append(&my_list, &m2);
    member_list_append(&my_list, &m3);
    member_list_append(&my_list, &m4);
    member_list_append(&my_list, &m5);
    member_list_append(&my_list, &m6);
    member_list_append(&my_list, &m7);
    member_list_append(&my_list, &m8);
    member_list_print(&my_list);

    //serialize list
    char *b;
    size_t size = member_list_serialize(&my_list, arena, &b);
    printf("my_list serialized size: %ld\n", size);

    //deserialize into other list
    struct member_list other_list;
    member_list_init(&other_list);
    member_list_deserialize(b, &other_list);
    member_list_print(&other_list);

    //check that members are what we expect
}

int main(int argc, char **argv) {
    g_arena = malloc(sizeof(struct arena));
    arena_init(g_arena, malloc, realloc, free);

    msg_queue = arena_malloc(g_arena, sizeof(struct msg_queue));
    msg_queue_init(msg_queue);

    g_member_list = arena_malloc(g_arena, sizeof(struct member_list));
    member_list_init(g_member_list);

    //member_list_serialization_test(&msg_queue->arena);

    struct port my_port = { .len=strlen(argv[1]) };
    memcpy(my_port.ptr, argv[1], my_port.len);
    if (is_introducer(&my_port)) {
        struct member me = { .port=my_port };
        member_list_append(g_member_list, &me);
    } else {
        struct port introducer_port = get_introducer();
        struct string payload = { .ptr=NULL, .len=0 };
        struct msg msg = { .type=MT_JOIN_REQ, .src=my_port, .dst=introducer_port, .payload=payload };
        if (!node_send_msg(&msg, &msg_queue->arena)) {
            printf("Failed to connect to introducer\n");
            exit(1);
        }
    }

    pthread_t listener_thread;
    int result = pthread_create(&listener_thread, NULL, node_listen, argv[1]);
    if (result != 0) {
        printf("Failed to create listener thread\n");
        exit(1);
    }

    //TODO: make gossiper thread here

    while (true) {
        struct msg *msg;
        while (msg_queue_dequeue(msg_queue, &msg)) {
            //printf("handling msg\n");
            handle_msg(msg);
        }

        msg_queue_clear(msg_queue);        
    }

    return 0;
}
