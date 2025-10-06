#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "net.h"
#include "msg.h"
#include "arena.h"

#define SERVER_COUNT 8
#define LISTEN_PORT "4000"

pthread_mutex_t mtx;
char *ports[SERVER_COUNT] = { "3000", 
                "3001",
                "3002",
                "3003",
                "3004",
                "3005",
                "3006",
                "3007"};

bool done[SERVER_COUNT] = { false, false, false, false,
                            false, false, false, false };

bool has_whitespace(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (s[i] == ' ') {
            return true; 
        }
    }

    return false;
}

void *dgrep_listen(void *arg) {
    int serverfd = net_listen(LISTEN_PORT);

    struct arena arena;
    arena_init(&arena, malloc, realloc, free);

    while (true) {
        struct msg msg;
        if (node_recv_msg(serverfd, &arena, &msg)) {
            printf("[localhost:%.*s]:\n%s", 4, msg.src_port, msg.buf);
            fflush(stdout);
            for (int i = 0; i < SERVER_COUNT; i++) {
                if (memcmp(ports[i], msg.src_port, 4) == 0) {
                    pthread_mutex_lock(&mtx);
                    done[i] = true;
                    pthread_mutex_unlock(&mtx);
                }
            }
            arena_dealloc_all(&arena);

        }
    }
}

int main(int argc, char **argv) {

    if (pthread_mutex_init(&mtx, NULL)) {
        exit(1);
    }

    char buf[1024];
    buf[0] = '\0';
    for (int i = 1; i < argc; i++) {
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

    char grep_buf[1024];
    grep_buf[0] = '\0';
    strcat(grep_buf, "grep ");
    strcat(grep_buf, buf);

    struct msg send_msg;
    send_msg.len = strlen(grep_buf) + 1; //include \0
    send_msg.buf = grep_buf;
    send_msg.type = MT_DGREP;
    memcpy(send_msg.src_port, LISTEN_PORT, 4);

    struct arena arena;
    arena_init(&arena, malloc, realloc, free);

    //start listening for gossip
    pthread_t listener_thread;
    int result = pthread_create(&listener_thread, NULL, dgrep_listen, NULL);
    if (result != 0) {
        exit(1);
    }

    //send requests to nodes    
    for (int i = 0; i < SERVER_COUNT; i++) {
        memcpy(send_msg.dst_port, ports[i], 4);
        if (!node_send_msg(&send_msg)) {
            printf("Failed to connect to [localhost:%s]\n", ports[i]);
            fflush(stdout);
            pthread_mutex_lock(&mtx);
            done[i] = true;
            pthread_mutex_unlock(&mtx);
        }
    }

    while (true) {
        /*
        bool all_done = true;
        for (int i = 0; i < SERVER_COUNT; i++) {
            if (!done[i]) {
                all_done = false;
            }
        }

        if (all_done) {
            break;
        }
        */

        bool all = true;
        pthread_mutex_lock(&mtx);
        for (int i = 0; i < SERVER_COUNT; i++) {
            if (!done[i]) {
                all = false;
            }
        }
        pthread_mutex_unlock(&mtx);
        if (all) {
            exit(0);
        }
    }

    return 0;
}


