#include "net.h"
#include "msg.h"
#include "member.h"
#include "util.h"
#include "arena.h"
#include <stdio.h>

#define INTRODUCER_PORT 3000
#define T_FAIL 32
#define T_CLEANUP T_FAIL * 2
#define T_GOSSIP 100000
#define T_INTRODUCER_MSG 30

struct msg_queue *msg_queue; //TODO: rename to g_msg_queue
struct arena *g_arena;
uint64_t g_heartbeat;
uint64_t g_timestamp;
port_t g_port;
struct member_list *g_member_list;

static inline bool is_introducer(port_t port) {
    return port == INTRODUCER_PORT;
}

static inline port_t get_introducer() {
    return INTRODUCER_PORT;
}


void update_own_heartbeat() {
    int idx;
    assert((idx = member_list_idx(g_member_list, g_port)) >= 0);
    g_member_list->values[idx].heartbeat = g_heartbeat;
    g_member_list->values[idx].timestamp = g_timestamp;
}

static inline bool member_failed(struct member *member) {
    return g_timestamp - member->timestamp >= T_FAIL;
}


static inline bool member_cleanup(struct member *member) {
    return g_timestamp - member->timestamp >= T_CLEANUP;
}

void member_list_cleanup(struct member_list *list) {
    int idx = 0;
    while (idx < list->count) {
        struct member *cur = &list->values[idx];
        if (member_cleanup(cur)) {
            printf("[%d]: peer left: ", g_port);
            member_print(cur);

            memcpy(cur, &list->values[list->count - 1], sizeof(struct member));
            list->count--;
        } else {
            idx++;        
        }
    }
}


void member_list_update(struct member_list *list, struct member *member) {
    int idx;
    if ((idx = member_list_idx(list, member->port)) >= 0) {
        struct member *cur = &list->values[idx];
        if (member->heartbeat > cur->heartbeat && !member_failed(cur)) {
            cur->heartbeat = member->heartbeat;
            cur->timestamp = g_timestamp;
        }
    } else {
        member->timestamp = g_timestamp;
        member_list_append(list, member);

        printf("[%d]: peer join: ", g_port);
        member_print(&list->values[list->count - 1]);
    }
}

bool member_list_random_entry(struct member_list *list, struct member *result) {
    assert(list->count > 0);
    if (list->count == 1) {
        return false; 
    }

    while (true) {
        int r = randint(0, list->count - 1);
        struct member *cur = &list->values[r];
        if (cur->port != g_port) {
            *result = *cur;
            return true;
        }
    }

    assert(false);
    return true;  
}

void member_list_merge(struct member_list *list, struct member_list *other) {
    for (int i = 0; i < other->count; i++) {
        struct member cur = other->values[i];
        member_list_update(list, &cur);
    }
}

int member_list_failed_members(const struct member_list *list) {
    int count = 0;
    for (int i = 0; i < list->count; i++) {
        struct member *m = &list->values[i]; 
        if (member_failed(m)) {
            count++;
        }
    }

    return count;
}

size_t serialize_nonfailed_members(const struct member_list *list, struct arena *arena, char **out_buf) {
    struct member_list nonfailed;
    member_list_init(&nonfailed, arena);
    for (int i = 0; i < list->count; i++) {
        struct member *m = &list->values[i]; 
        if (!member_failed(m)) {
            member_list_append(&nonfailed, m);
        }
    }

    return member_list_serialize(&nonfailed, arena, out_buf);
}

void *introducer_messager(void *arg) {
    while (true) {
        port_t introducer_port = get_introducer();
        char *buf;
        size_t size = serialize_nonfailed_members(g_member_list, &msg_queue->arena, &buf);
        struct string payload = { .ptr=buf, .len=size };
        struct msg msg = { .type=MT_JOIN_REQ, .src=g_port, .dst=introducer_port, .payload=payload };
        if (!node_send_msg(&msg, &msg_queue->arena)) {
            //do nothing
        }

        sleep(randint(0, T_INTRODUCER_MSG));
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

void *node_udp_listen(void *arg) {
    struct udp_listener ul;
    int serverfd = net_udp_listen(arg, &ul);
    struct arena arena;
    arena_init(&arena, malloc, realloc, free);

    while (true) {
        struct msg msg;
        //TODO: node_recv_msg won't work with stream socket???
        //if (node_recv_msg(serverfd, &arena, &msg)) {
        if (node_recvfrom_msg(serverfd, &arena, &msg)) {
            //print_msg(&msg);
            msg_queue_enqueue(msg_queue, &msg);
        }
        arena_dealloc_all(&arena);
    }
}

void *node_gossip(void *arg) {
    struct arena arena;
    arena_init(&arena, malloc, realloc, free);
    while (true) {
        usleep(T_GOSSIP);
        g_heartbeat++;
        g_timestamp++;

        update_own_heartbeat();

        char *buf;
        size_t size = serialize_nonfailed_members(g_member_list, &msg_queue->arena, &buf);
        struct member m;
        if (member_list_random_entry(g_member_list, &m)) {
            struct msg msg = { .type=MT_HEARTBEAT, .src=g_port, .dst=m.port, .payload=(struct string) { .ptr=buf, .len=size } };
            if (node_sendto_msg(&msg, &arena)) { 
                //don't care if it fails - will try again next iteration
            }
        }
        arena_dealloc_all(&arena);


        member_list_cleanup(g_member_list);
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
        struct member_list peer_list;
        member_list_init(&peer_list, &msg_queue->arena);
        member_list_deserialize(msg->payload.ptr,&peer_list);
        member_list_merge(g_member_list, &peer_list);


        char *buf;
        size_t size = serialize_nonfailed_members(g_member_list, &msg_queue->arena, &buf);
        struct msg res_msg = { .type=MT_JOIN_RES, .src=msg->dst, .dst=msg->src, .payload=(struct string) { .ptr=buf, .len=size } };
        if (!node_send_msg(&res_msg, &msg_queue->arena)) {
            printf("failed to connect\n");
        } else {
            //printf("sent response\n");
        }
        break;
    }
    case MT_JOIN_RES: {
        struct member_list peer_list;
        member_list_init(&peer_list, &msg_queue->arena);
        member_list_deserialize(msg->payload.ptr,&peer_list);
        member_list_merge(g_member_list, &peer_list);
        break;
    }
    case MT_HEARTBEAT: {
        struct member_list peer_list;
        member_list_init(&peer_list, &msg_queue->arena);
        member_list_deserialize(msg->payload.ptr,&peer_list);
        member_list_merge(g_member_list, &peer_list);
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
    printf("expect 3000: %d\n", n.port);
}


void member_list_serialization_test(struct arena *arena) {
    struct member_list my_list;
    member_list_init(&my_list, arena);
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
    member_list_update(&my_list, &m0);
    member_list_update(&my_list, &m1);
    member_list_update(&my_list, &m2);
    member_list_update(&my_list, &m3);
    member_list_update(&my_list, &m4);
    member_list_update(&my_list, &m5);
    member_list_update(&my_list, &m6);
    member_list_update(&my_list, &m7);
    member_list_update(&my_list, &m8);
    member_list_print(&my_list);

    //serialize list
    char *b;
    size_t size = member_list_serialize(&my_list, arena, &b);
    printf("my_list serialized size: %ld\n", size);

    //deserialize into other list
    struct member_list other_list;
    member_list_init(&other_list, arena);
    member_list_deserialize(b, &other_list);
    member_list_print(&other_list);

    //check that members are what we expect
}

int main(int argc, char **argv) {
    g_heartbeat = 0;
    g_timestamp = 0;
    g_arena = malloc(sizeof(struct arena));
    arena_init(g_arena, malloc, realloc, free);

    msg_queue = arena_malloc(g_arena, sizeof(struct msg_queue));
    msg_queue_init(msg_queue);

    g_member_list = arena_malloc(g_arena, sizeof(struct member_list));
    member_list_init(g_member_list, g_arena);

    char *endptr;
    g_port = strtoul(argv[1], &endptr, 10);
    struct member me = { .port=g_port, .heartbeat=g_heartbeat, .timestamp=g_timestamp };
    member_list_update(g_member_list, &me);

    if (!is_introducer(g_port)) {
        pthread_t introducer_msgr_thread;
        int result = pthread_create(&introducer_msgr_thread, NULL, introducer_messager, NULL);
        if (result != 0) {
            printf("Failed to create listener thread\n");
            exit(1);
        }
    }

    pthread_t listener_thread;
    int result = pthread_create(&listener_thread, NULL, node_listen, argv[1]);
    if (result != 0) {
        printf("Failed to create listener thread\n");
        exit(1);
    }

    {
        pthread_t udp_listener_thread;
        int result = pthread_create(&udp_listener_thread, NULL, node_udp_listen, argv[1]);
        if (result != 0) {
            printf("Failed to create udp listener thread\n");
            exit(1);
        }
    }

    {
        pthread_t gossiper_thread;
        int result = pthread_create(&gossiper_thread, NULL, node_gossip, NULL);
        if (result != 0) {
            printf("Failed to create gossiper thread\n");
            exit(1);
        }
    }
    

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
