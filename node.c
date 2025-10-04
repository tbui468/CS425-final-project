#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include "net.h"
#include "msg.h"
#include "arena.h"

#define INTRODUCER_PORT "3000"


struct member {
    char port[4];
};

size_t member_serialized_size() {
    return 4; //just 4 digits for port for now
}

struct member_list {
    struct member *values;
    int capacity;
    int count;
};

void member_list_init(struct member_list *list) {
    list->capacity = 8;
    list->values = malloc(sizeof(struct member) * list->capacity);
    list->count = 0;
}

void member_list_append(struct member_list *list, struct member *member) {
    assert(list->count < list->capacity);

    list->values[list->count] = *member;
    list->count++;
}

void member_list_print(struct member_list *list) {
    for (int i = 0; i < list->count; i++) {
        printf("port: %.*s\n", 4, list->values[i].port);
    }
}

size_t member_list_serialize(struct member_list *list, char **out_buf, struct arena *arena) {
    uint32_t count = list->count;
    size_t size = sizeof(uint32_t) + count * member_serialized_size();
    char *buf = arena_malloc(arena, size);
    memcpy(buf, &count, sizeof(uint32_t));
    for (int i = 0; i < list->count; i++) {
        memcpy(buf + sizeof(uint32_t) + i * member_serialized_size(), list->values[i].port, 4);
    }
    *out_buf = buf;
    return size;
}

void member_list_deserialize(struct member_list *list, char *buf, size_t len) {
    printf("deserializing\n");
    free(list->values);
    uint32_t count = *((uint32_t*) buf);
    printf("read count; %d\n", count);
    list->count = count;
    list->capacity = 8;
    while (list->count > list->capacity) {
        list->capacity *= 2;
    }
    list->values = malloc(sizeof(struct member) * list->capacity);
   
    for (int i = 0; i < count; i++) {
        memcpy(list->values[i].port, buf + sizeof(uint32_t) + i * member_serialized_size(), 4);
    }  
}

struct node {
    const char *port;
    struct member_list members;
};

void *node_listen(void *arg) {
    struct node *node = (struct node*) arg;

    int serverfd = net_listen(node->port);

    char filename[64];
    sprintf(filename, "machine.%s.log", node->port);
    FILE *f = fopen(filename, "a");
    if (f == NULL) {
        f = fopen(filename, "w");
    }
    if (f == NULL) {
        exit(1);
    }

    fprintf(f, "Listening on [localhost:%s]\n", node->port);
    fflush(f);

    struct arena arena;
    arena_init(&arena, malloc, realloc, free);

    while (true) {
        int connfd = net_accept(serverfd);
        if (connfd == -1)
            continue;
        fprintf(f, "Client connected\n"); 
        fflush(f);

        struct msg msg;
        msg.sockfd = connfd;
        if (!node_recv_msg(&msg, &arena)) {
            net_disconnect(connfd);
            continue; 
        }

        printf("message type: %d\n", msg.type);

        //TODO: check message type here and handle it accordingly
        switch (msg.type) {
        case MT_DGREP: {
            //run grep locally
            FILE * cmd = popen(msg.buf, "r");
            if (cmd == NULL) {
                net_disconnect(msg.sockfd);
                exit(1);
            }

            //read local grep results
            size_t capacity = 1024;
            char *result = arena_malloc(&arena, capacity);
            result[0] = '\0';
            size_t size = strlen(result);
            while (fgets(result + size, capacity - size, cmd)) {
                size = strlen(result);
                if (size == capacity - 1) {
                    capacity *= 2;
                    result = arena_realloc(&arena, result, capacity);
                }
            }

            pclose(cmd); 

            //send result back to client
            struct msg res_msg;
            res_msg.type = MT_DGREP;
            res_msg.sockfd = connfd;
            res_msg.buf = result;
            res_msg.len = size + 1;
            node_send_msg(&res_msg);
            
            break;
        }
        case MT_JOIN_REQ: {
            struct msg res_msg;
            res_msg.type = MT_JOIN_RES;
            res_msg.sockfd = connfd;
            printf("serializing\n");

            struct member member;
            assert(msg.len == 4);
            memcpy(member.port, msg.buf, msg.len);
            member_list_append(&node->members, &member);
            res_msg.len = member_list_serialize(&node->members, &res_msg.buf, &arena);
            printf("done serializing\n");
            node_send_msg(&res_msg);
            printf("done sending\n");
            break;
        }
        case MT_JOIN_RES:
            break;
        }

        
        net_disconnect(connfd);

        arena_dealloc_all(&arena);
    }
}

bool is_introducer(const char *port) {
    return strncmp(INTRODUCER_PORT, port, 4) == 0;
}

int main(int argc, char **argv) {
    assert(argc == 2);



    struct node node;
    node.port = argv[1];
    node.members.values = NULL;
    struct arena arena;
    arena_init(&arena, malloc, realloc, free);

    if (!is_introducer(argv[1])) {
        //TODO: put this in loop to allow retrying if introducer fails

        int introducer_sockfd = net_connect("localhost", INTRODUCER_PORT);

        struct msg introduction;
        introduction.sockfd = introducer_sockfd;
        introduction.type = MT_JOIN_REQ;
        introduction.buf = arena_strdup(&arena, argv[1]);
        introduction.len = strlen(introduction.buf);
        node_send_msg(&introduction);

        struct msg reply;
        reply.sockfd = introducer_sockfd;
        assert(node_recv_msg(&reply, &arena));
        assert(reply.type == MT_JOIN_RES);
        member_list_deserialize(&node.members, reply.buf, reply.len);
        member_list_print(&node.members);
        
        net_disconnect(introducer_sockfd);
    } else {
        struct member member;
        assert(strlen(argv[1]) == 4);
        memcpy(member.port, argv[1], 4);
        member_list_init(&node.members);
        member_list_append(&node.members, &member);
    }

    arena_dealloc_all(&arena);

    //start listening for gossip
    pthread_t listener_thread;
    int result = pthread_create(&listener_thread, NULL, node_listen, &node);
    if (result != 0) {
        exit(1);
    }

    while (true) {
        //TODO: check message queue and handle any message here.  Listening thread should only queue messages (be sure to lock queue when appending)
        //TODO: periodically send gossip (think about what time-bound we want to achieve, include own heartbeat in gossip membership list) using UDP
    }

    return 0;
}


