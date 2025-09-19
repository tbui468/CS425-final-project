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

    fprintf(f, "server on port %s listening\n", argv[1]);
    fflush(f);

    while (true) {
        int connfd = net_accept(serverfd);
        if (connfd == -1)
            continue;
        fprintf(f, "client connected\n"); 
        fflush(f);
    }
    return 0;
}


