#include "./../net.h"
#include "./../msg.h"
#include <stdio.h>

struct msg_queue *msg_queue;

void *node_listen(void *arg) {
    int serverfd = net_listen(arg);
    struct arena arena;
    arena_init(&arena, malloc, realloc, free);

    while (true) {
        struct msg msg;
        if (node_recv_msg(serverfd, &arena, &msg)) {
            print_msg(&msg);
            msg_queue_enqueue(msg_queue, &msg);
        }
        arena_dealloc_all(&arena);
    }
}

void handle_msg(struct msg *msg) {
    switch (msg->type) {
    case MT_GREP_REQ: {

        //run grep locally
        /*
        char *cstring = arena_malloc(&msg_queue->arena, msg->len);
        memcpy(cstring, msg->buf, msg->len);
        cstring[msg->len] = '\0';
        */

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
        res_msg.src_port = string_dup(&msg->dst_port, &msg_queue->arena);
        res_msg.dst_port = string_dup(&msg->src_port, &msg_queue->arena);
        res_msg.payload = (struct string) { .ptr = result, .len = size };
        if (!node_send_msg(&res_msg, &msg_queue->arena)) {
            printf("failed to connect\n");
        }
        break;
    }
    default:
        printf("message handler not implemented\n");
        break;
    }
}

int main(int argc, char **argv) {
    msg_queue = malloc(sizeof(struct msg_queue));
    msg_queue_init(msg_queue);

    pthread_t listener_thread;
    int result = pthread_create(&listener_thread, NULL, node_listen, argv[1]);
    if (result != 0) {
        exit(1);
    }

    while (true) {
        struct msg *msg;
        while (msg_queue_dequeue(msg_queue, &msg)) {
            printf("handling msg\n");
            handle_msg(msg);
        }

        msg_queue_clear(msg_queue);        
    }

    return 0;
}
