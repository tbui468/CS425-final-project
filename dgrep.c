#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "net.h"
#include "msg.h"
#include "arena.h"

#define SERVER_COUNT 8

bool has_whitespace(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (s[i] == ' ') {
            return true; 
        }
    }

    return false;
}

int main(int argc, char **argv) {
    int sockfds[SERVER_COUNT];
    char *ports[SERVER_COUNT] = { "3000", 
                    "3001",
                    "3002",
                    "3003",
                    "3004",
                    "3005",
                    "3006",
                    "3007"};

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

    struct arena arena;
    arena_init(&arena, malloc, realloc, free);
    
    
    for (int i = 0; i < SERVER_COUNT; i++) {
        sockfds[i] = net_connect("localhost", ports[i]);
        printf("connected\n");
        if (sockfds[i] > 0) {
            send_msg.sockfd = sockfds[i];
            node_send_msg(&send_msg);


            struct msg msg;
            msg.sockfd = sockfds[i];
            if (node_recv_msg(&msg, &arena)) {
                printf("[localhost:%s]:\n%s", ports[i], msg.buf);
                fflush(stdout);
            } else {
                printf("Connection to [localhost:%s] ended\n", ports[i]);
                fflush(stdout);
            }
            net_disconnect(sockfds[i]);
        } else {
            printf("Failed to connect to [localhost:%s]\n", ports[i]);
            fflush(stdout);
        }
    }

    return 0;
}


