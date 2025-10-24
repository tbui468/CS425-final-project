#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>


#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

/*
    Network code is mostly copied from this guide:
    https://beej.us/guide/bgnet/
*/

#define BACKLOG 16

/*
    client
*/

int net_connect(const char* hostname, const char* port) {
    int sockfd;

    struct addrinfo hints;
    struct addrinfo* servinfo;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, port, &hints, &servinfo) != 0)
        return -1;

    //connect to first result possible
    struct addrinfo* p;

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL)
        return -1;

    freeaddrinfo(servinfo);

    return sockfd;
}

void net_disconnect(int sockfd) {
    close(sockfd);
}

void net_send(int sockfd, char* buf, int len) {
    ssize_t written = 0;
    ssize_t n;

    while (written < len) {
        if ((n = send(sockfd, buf + written, len - written, 0)) <= 0) {
            if (n < 0 && errno == EINTR) {//interrupted but not error, so we need to try again
                n = 0;
            } else {
                exit(1); //real error
            }
        }

        written += n;
    }
}

void net_send_msg(int sockfd, char* buf, size_t len) {
    uint32_t fixed_len = len;
    net_send(sockfd, (char *) &fixed_len, sizeof(uint32_t));
    net_send(sockfd, buf, len); 
}

bool net_recv(int sockfd, char* buf, int len) {
    ssize_t nread = 0;
    ssize_t n;
    while (nread < len) {
        if ((n = recv(sockfd, buf + nread, len - nread, 0)) < 0) {
            if (n < 0 && errno == EINTR)
                n = 0;
            else
                exit(1);
        } else if (n == 0) {
            //connection ended
            return false;
        }

        nread += n;
    }

    return true;
}

bool net_recv_msg(int sockfd, void* (*allocator)(size_t), char **out_buf, size_t *len) {
    uint32_t fixed_len;
    if (!net_recv(sockfd, (char *) &fixed_len, sizeof(uint32_t))) {
        return false;
    }

    *len = fixed_len;
    char *buf = allocator(*len);
    *out_buf = buf;
    if (!net_recv(sockfd, buf, *len)) {
        free(buf);
        return false;
    }

    return true;
}

/*
    server
*/

void net_sigchld_handler(int s) {
    s = s; //silence warning

    int saved_errno = errno;

    while (waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

int net_send_datagram(const char *host, const char *port, const void *buf, size_t len) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    rv = getaddrinfo(host, port, &hints, &servinfo);
    if (rv != 0) {
        return false;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }
        break;
    }

    if (p == NULL) {
        return false;
    }
    ssize_t n;

    if ((n = sendto(sockfd, buf, len, 0, p->ai_addr, p->ai_addrlen)) <= 0) {
        freeaddrinfo(servinfo);
        close(sockfd);
        return false;
    }


    freeaddrinfo(servinfo);
    close(sockfd);
    return true;
}

int net_recvfrom(int sockfd, void *buf, size_t len) {
    struct sockaddr_storage their_addr;
    socklen_t addr_len = sizeof(their_addr);
    ssize_t n;

    if ((n = recvfrom(sockfd, buf, len, 0, (struct sockaddr *) &their_addr, &addr_len)) != -1) {
        return true;
    }

    return false;
}

struct udp_listener {
    int sockfd;
    struct addrinfo *p;
    struct addrinfo *root;
};

int net_udp_listen(const char* port, struct udp_listener *out) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; //use local ip address

    struct addrinfo* servinfo;
    if (getaddrinfo(NULL, port, &hints, &servinfo) != 0) {
        exit(1);
    }

    //loop through results for getaddrinfo and bind to first one we can
    struct addrinfo* p;
    int sockfd;
    int yes = 1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
            exit(1);

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    //freeaddrinfo(servinfo); //TODO: not freeining since this is needed for recvfrom

    //failed to bind
    if (p == NULL) {
        exit(1);
    }

    struct udp_listener data = { .sockfd=sockfd, .p=p, .root=servinfo };
    *out = data; 

    return sockfd;
}

int net_listen(const char* port) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; //use local ip address

    struct addrinfo* servinfo;
    if (getaddrinfo(NULL, port, &hints, &servinfo) != 0) {
        return 1;
    }

    //loop through results for getaddrinfo and bind to first one we can
    struct addrinfo* p;
    int sockfd;
    int yes = 1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
            exit(1);

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    //failed to bind
    if (p == NULL) {
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        exit(1);
    }

    //what was this for again?
    struct sigaction sa;
    sa.sa_handler = net_sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        exit(1);
    }

    return sockfd;
}

int net_accept(int sockfd) {
    socklen_t sin_size = sizeof(struct sockaddr_storage);
    struct sockaddr_storage their_addr;
    int client_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);

    return client_fd;
}

#endif //NET_H
