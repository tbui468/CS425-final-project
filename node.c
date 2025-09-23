#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "net.h"

int main(int argc, char **argv) {
    assert(argc == 2);
    int serverfd = net_listen(argv[1]);

    char filename[64];
    sprintf(filename, "machine.%s.log", argv[1]);
    FILE *f = fopen(filename, "a");
    if (f == NULL) {
        f = fopen(filename, "w");
    }
    if (f == NULL) {
        exit(1);
    }

    fprintf(f, "Listening on [localhost:%s]\n", argv[1]);
    fflush(f);

    while (true) {
        int connfd = net_accept(serverfd);
        if (connfd == -1)
            continue;
        fprintf(f, "Client connected\n"); 
        fflush(f);
        size_t len;
        char buf[1024];
        net_recv_msg(connfd, buf, &len);
        //fprintf(f, "grep arguments: %s\n", buf);
        //fflush(f);

        //TODO: run grep
        //send grep results back to client

        FILE * cmd = popen(buf, "r");
        if (cmd == NULL) {
            exit(1);
        }
        char result[1024];
        result[0] = '\0';
        size_t off = 0;
        while (fgets(result + off, sizeof(result), cmd)) {
            off = strlen(result);
            //fprintf(f, "[%s] %s", argv[1], result);
            //fflush(f);
        }
        pclose(cmd); 
        net_send_msg(connfd, result, strlen(result) + 1);
        net_disconnect(connfd);
    }
    return 0;
}


