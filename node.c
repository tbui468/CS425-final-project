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
        char *buf;

        if (!net_recv_msg(connfd, malloc, &buf, &len)) {
            net_disconnect(connfd);
            continue; 
        }

        //run grep locally
        FILE * cmd = popen(buf, "r");
        if (cmd == NULL) {
            net_disconnect(connfd);
            exit(1);
        }

        free(buf);

        //read local grep results
        size_t capacity = 1024;
        char *result = malloc(capacity);
        result[0] = '\0';
        size_t size = strlen(result);
        while (fgets(result + size, capacity - size, cmd)) {
            size = strlen(result);
            if (size == capacity - 1) {
                capacity *= 2;
                result = realloc(result, capacity);
            }
        }

        //send result back to client
        net_send_msg(connfd, result, size + 1);
        free(result);
        
        pclose(cmd); 
        net_disconnect(connfd);
    }
    return 0;
}


