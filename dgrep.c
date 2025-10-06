#include "net.h"
#include "msg.h"
#include <stdio.h>

#define SERVER_COUNT 8
#define LISTENING_PORT "4000"

bool complete[SERVER_COUNT] = { 
                                false, false, false, false,
                                false, false, false, false
                                };


const char *ports[SERVER_COUNT] = { 
                                    "3000", "3001", "3002", "3003",
                                    "3004", "3005", "3006", "3007"
                                    };

struct msg_queue *msg_queue;

void set_complete_status(struct string port) {
    for (int i = 0; i < SERVER_COUNT; i++) {
        if (memcmp(ports[i], port.ptr, port.len) == 0) {
            complete[i] = true;
            break;
        }
    }

    int count = 0;
    for (int i = 0; i < SERVER_COUNT; i++) {
        count += complete[i]; 
    }
    if (count == SERVER_COUNT) {
        exit(0);
    }
}

void handle_msg(struct msg *msg) {
    switch (msg->type) {
    case MT_GREP_RES: {
        printf("[%.*s]:\n%.*s", (int) msg->src_port.len, msg->src_port.ptr, (int) msg->payload.len, msg->payload.ptr);
        set_complete_status(msg->src_port);

        break;  
    }
    default:
        assert(false);
        break;
    }
}

bool has_whitespace(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (s[i] == ' ') {
            return true; 
        }
    }

    return false;
}


size_t generate_grep_command(int argc, char **argv, struct arena *arena, char **out_buf) {
    static char buf[1024];
    buf[0] = '\0';
    strcat(buf, "grep ");

    for (int i = 0; i < argc; i++) {
        bool add_quotes = has_whitespace(argv[i], strlen(argv[i]));
        if (add_quotes) {
            strcat(buf, "\"");
        }
        strcat(buf, argv[i]);
        if (add_quotes) {
            strcat(buf, "\"");
        }
        strcat(buf, " ");
    }

    *out_buf = buf;
    return strlen(buf);
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

int main(int argc, char **argv) {
    msg_queue = malloc(sizeof(struct msg_queue));
    msg_queue_init(msg_queue);

    pthread_t listener_thread;
    int result = pthread_create(&listener_thread, NULL, node_listen, LISTENING_PORT);
    if (result != 0) {
        exit(1);
    }

    char *grep_buf;
    size_t grep_len = generate_grep_command(argc - 1, &argv[1], &msg_queue->arena, &grep_buf);
    //printf("grep: %.*s\n", (int) grep_len, grep_buf);
    
    for (int i = 0; i < SERVER_COUNT; i++) {
        struct msg msg;
        msg.type = MT_GREP_REQ;
        msg.src_port = string_from_cstring(LISTENING_PORT, &msg_queue->arena);
        msg.dst_port = string_from_cstring(ports[i], &msg_queue->arena);
        msg.payload = (struct string) { .ptr = grep_buf, .len = grep_len };
        if (!node_send_msg(&msg, &msg_queue->arena)) {
            printf("failed to connect\n");
            set_complete_status(msg.dst_port);
        } else {
            printf("connected\n");
        }
    }

    while (true) {
        struct msg *msg;
        while (msg_queue_dequeue(msg_queue, &msg)) {
            //printf("handling msg\n");
            //print_msg(&msg);
            handle_msg(msg);
        }

        msg_queue_clear(msg_queue);        
    }

    return 0;
}
