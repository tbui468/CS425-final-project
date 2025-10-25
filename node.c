#include "net.h"
#include "msg.h"
#include "membership.h"
#include "arena.h"
#include "common.h"
#include <stdio.h>
#include <pthread.h>

#define INTRODUCER_PORT 3000
#define T_GOSSIP 500000 / 1000 //make this 1000 to see race condition //TODO
#define T_TIMESTAMP 250000 / 1000 //make this 1000 to see race condition //TODO
#define T_INTRODUCER_MSG 8 //period between contacting introducer (to ensure failed introducers accepted back into group on rejoining)

struct msg_queue *msg_queue; //TODO: rename to g_msg_queue
struct arena *g_arena;
uint64_t g_heartbeat;
uint64_t g_timestamp;
port_t g_port;
struct member_list *g_member_list;
pthread_mutex_t g_memberlist_mtx;
bool g_joined;

static inline bool is_introducer(port_t port) {
    return port == INTRODUCER_PORT;
}

static inline port_t get_introducer() {
    return INTRODUCER_PORT;
}

void *introducer_messager(void *arg) {
    struct arena arena;
    arena_init(&arena, malloc, realloc, free);
    while (true) {
        port_t introducer_port = get_introducer();
        char *buf;
        size_t size = serialize_nonfailed_members(g_member_list, &arena, &buf, g_timestamp);
        struct string payload = { .ptr=buf, .len=size };
        struct msg msg = { .type=MT_JOIN_REQ, .src=g_port, .dst=introducer_port, .payload=payload };
        if (!node_send_msg(&msg, &arena)) {
            //do nothing
        }
        arena_dealloc_all(&arena);

        sleep(T_INTRODUCER_MSG);
    }
}

void *node_listen(void *arg) {
    int serverfd = net_listen(arg);
    struct arena arena;
    arena_init(&arena, malloc, realloc, free);

    while (true) {
        struct msg msg;
        if (node_recv_msg(serverfd, &arena, &msg)) {
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
        if (node_recvfrom_msg(serverfd, &arena, &msg)) {
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

        update_own_heartbeat(g_member_list, g_port, g_timestamp, g_heartbeat);

        char *buf;
        //size_t size = serialize_nonfailed_members(g_member_list, &msg_queue->arena, &buf, g_timestamp);
        size_t size = serialize_nonfailed_members(g_member_list, &arena, &buf, g_timestamp);

        struct member m;
        if (member_list_random_entry(g_member_list, &m, g_port)) {
            struct msg msg = { .type=MT_HEARTBEAT, .src=g_port, .dst=m.port, .payload=(struct string) { .ptr=buf, .len=size } };
            if (node_sendto_msg(&msg, &arena)) { 
                //don't care if it fails - will try again next gossip iteration
            }
        }
        arena_dealloc_all(&arena);


        member_list_cleanup(g_member_list, g_timestamp);
    }
}


void *update_timestamp(void *arg) {
    while (true) {
        usleep(T_TIMESTAMP);
        g_timestamp++;
    }
}

void handle_msg(struct msg *msg, struct arena *arena) {
    switch (msg->type) {
    case MT_GREP_REQ: {
        print_msg(msg);

        char *cstring = string_to_cstring(&msg->payload, arena);

        FILE * cmd = popen(cstring, "r");
        if (cmd == NULL) {
            exit(1);
        }

        //read local grep results
        size_t capacity = 32;
        char *result = arena_malloc(arena, capacity);
        result[0] = '\0';
        size_t size = strlen(result);
        while (fgets(result + size, capacity - size, cmd)) {
            size = strlen(result);
            if (size == capacity - 1) {
                capacity *= 2;
                result = arena_realloc(arena, result, capacity);
            }
        }

        pclose(cmd); 

        //construct and send response
        struct msg res_msg;
        res_msg.type = MT_GREP_RES;
        res_msg.src = msg->dst;
        res_msg.dst = msg->src;
        res_msg.payload = (struct string) { .ptr = result, .len = size };
        if (!node_send_msg(&res_msg, arena)) {
            printf("failed to connect\n");
        } else {
            printf("sent response\n");
        }
        break;
    }
    case MT_JOIN_REQ: {
        struct member_list peer_list;
        member_list_init(&peer_list, arena, NULL);
        member_list_deserialize(msg->payload.ptr,&peer_list);
        member_list_merge(g_member_list, &peer_list, g_timestamp);


        char *buf;
        size_t size = serialize_nonfailed_members(g_member_list, arena, &buf, g_timestamp);
        struct msg res_msg = { .type=MT_JOIN_RES, .src=msg->dst, .dst=msg->src, .payload=(struct string) { .ptr=buf, .len=size } };
        if (!node_send_msg(&res_msg, arena)) {
            printf("failed to connect\n");
        } else {
            //printf("sent response\n");
        }
        break;
    }
    case MT_JOIN_RES: {
        struct member_list peer_list;
        member_list_init(&peer_list, arena, NULL);
        member_list_deserialize(msg->payload.ptr,&peer_list);
        member_list_merge(g_member_list, &peer_list, g_timestamp);
        break;
    }
    case MT_HEARTBEAT: {
        struct member_list peer_list;
        member_list_init(&peer_list, arena, NULL);
        member_list_deserialize(msg->payload.ptr,&peer_list);
        member_list_merge(g_member_list, &peer_list, g_timestamp);
        break; 
    }
    default:
        printf("message handler not implemented\n");
        break;
    }
}

int main(int argc, char **argv) {
    g_heartbeat = 0;
    g_timestamp = 0;
    g_arena = malloc(sizeof(struct arena));
    arena_init(g_arena, malloc, realloc, free);

    msg_queue = arena_malloc(g_arena, sizeof(struct msg_queue));
    msg_queue_init(msg_queue);

    g_member_list = arena_malloc(g_arena, sizeof(struct member_list));
    if (pthread_mutex_init(&g_memberlist_mtx, NULL)) {
        exit(1);
    }
    member_list_init(g_member_list, g_arena, &g_memberlist_mtx);

    char *endptr;
    g_port = strtoul(argv[1], &endptr, 10);
    g_joined = false;
    log_init(g_port);
    struct member me = { .port=g_port, .heartbeat=g_heartbeat, .timestamp=g_timestamp };
    member_list_update(g_member_list, &me, g_timestamp);



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

    {
        pthread_t timestamp_thread;
        int result = pthread_create(&timestamp_thread, NULL, update_timestamp, NULL);
        if (result != 0) {
            printf("Failed to create timestamp thread\n");
            exit(1);
        }
    }
    
    struct arena arena;
    arena_init(&arena, malloc, realloc, free);
    while (true) {
        struct msg *msg;
        while (msg_queue_dequeue(msg_queue, &msg)) {
            handle_msg(msg, &arena);
            arena_dealloc_all(&arena);
            
        }

        msg_queue_clear(msg_queue);        
    }

    return 0;
}
