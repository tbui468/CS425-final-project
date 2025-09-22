#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "net.h"

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
    
    /*
    FILE * cmd = popen(grep_buf, "r");
    if (cmd == NULL) {
        exit(1);
    }
    char result[1024];
    while (fgets(result, sizeof(result), cmd)) {
        printf("output: %s", result);
    }
    pclose(cmd); 
    */

    
    for (int i = 0; i < SERVER_COUNT; i++) {
        sockfds[i] = net_connect("localhost", ports[i]);
        if (sockfds[i] > 0) {
            size_t grep_len = strlen(grep_buf) + 1; //include \0
            net_send_msg(sockfds[i], grep_buf, (int) grep_len);

            //TODO: receive grep response and output
            size_t len;
            char buf[1024];
            net_recv_msg(sockfds[i], buf, &len);
            printf("[localhost:%s]:\n%s", ports[i], buf);
            net_disconnect(sockfds[i]);
        }
    }

    return 0;
}


