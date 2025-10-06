#ifndef MSG_H
#define MSG_H

#include <unistd.h> //for usleep
#include <pthread.h>
#include <stdio.h>
#include "arena.h"
#include "net.h"
#include "string.h"

enum msg_type {
    MT_GREP_REQ,
    MT_GREP_RES,
    MT_JOIN_REQ,
    MT_JOIN_RES
};

struct msg {
    enum msg_type type;
    int sockfd; 
    struct string src_port;
    struct string dst_port;
    struct string payload;
};

void print_msg(struct msg *msg) {
    printf("Type: %d\n", msg->type);
    printf("Src: %.*s\n", (int) msg->src_port.len, msg->src_port.ptr);
    printf("Dst: %.*s\n", (int) msg->dst_port.len, msg->dst_port.ptr);
    printf("Payload: %.*s\n", (int) msg->payload.len, msg->payload.ptr);
}

void msg_copy(const struct msg *msg, struct arena *arena, struct msg *copy) {
    copy->type = msg->type;
    copy->sockfd = msg->sockfd;
    copy->src_port = string_dup(&msg->src_port, arena);
    copy->dst_port = string_dup(&msg->dst_port, arena);
    copy->payload = string_dup(&msg->payload, arena);
}

static char *append_to_buf(char *buf, void *ptr, size_t len) {
    memcpy(buf, ptr, len);
    return buf + len;
}

//length: uint32_t
//type: uint8_t
//strings: src_port, dst_port, payload
size_t msg_serialize(const struct msg *msg, struct arena *arena, char **out_buf) {
    uint8_t msg_type = msg->type;
    uint32_t src_size = msg->src_port.len;
    uint32_t dst_size = msg->dst_port.len;
    uint32_t payload_size = msg->payload.len;
                          //total size       type              //src,dst,payload
    uint32_t total_size = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint32_t) * 3 + src_size + dst_size + payload_size;

    char *buf = arena_malloc(arena, total_size);
    *out_buf = buf;

    buf = append_to_buf(buf, &total_size, sizeof(uint32_t));
    buf = append_to_buf(buf, &msg_type, sizeof(uint8_t));
    buf = append_to_buf(buf, &src_size, sizeof(uint32_t));
    buf = append_to_buf(buf, msg->src_port.ptr, src_size);
    buf = append_to_buf(buf, &dst_size, sizeof(uint32_t));
    buf = append_to_buf(buf, msg->dst_port.ptr, dst_size);
    buf = append_to_buf(buf, &payload_size, sizeof(uint32_t));
    buf = append_to_buf(buf, msg->payload.ptr, payload_size);

    return total_size;
}

//steal allocated data rather than allocating new resources
bool msg_deserialize_resource(char *buf, struct msg *msg) {
    uint8_t msg_type = *((uint8_t*) buf );
    buf += sizeof(uint8_t);
    msg->type = msg_type;

    uint32_t src_size = *((uint32_t*) buf );
    buf += sizeof(uint32_t);
    msg->src_port.len = src_size;
    msg->src_port.ptr = buf;
    buf += src_size;    

    uint32_t dst_size = *((uint32_t*) buf );
    buf += sizeof(uint32_t);
    msg->dst_port.len = dst_size;
    msg->dst_port.ptr = buf;
    buf += dst_size;    

    uint32_t payload_size = *((uint32_t*) buf );
    buf += sizeof(uint32_t);
    msg->payload.len = payload_size;
    msg->payload.ptr = buf;
    buf += payload_size;

    return true;
}


bool node_send_msg(struct msg *msg, struct arena *arena) {
    char buf[8];
    memcpy(buf, msg->dst_port.ptr, msg->dst_port.len);
    buf[msg->dst_port.len] = '\0';

    int attempts = 0;
    while (attempts < 4) {
        int sockfd = net_connect("localhost", buf);
        if (sockfd <= 0) {
            attempts++;
            //usleep(100000); //0.1 seconds
            usleep(50000); //0.05 seconds
            continue;
        }

        char *serialized_msg;
        size_t msg_size = msg_serialize(msg, arena, &serialized_msg);
        net_send(sockfd, serialized_msg, msg_size);

        /*
        uint8_t fixed_type = (uint8_t) msg->type;
        uint32_t fixed_len = (uint32_t) msg->len;
        net_send(sockfd, (char *) &fixed_type, sizeof(uint8_t));
        uint8_t src_port_len = msg->src_port.len;
        net_send(sockfd, &src_port_len, sizeof(uint8_t));
        net_send(sockfd, msg->src_port.ptr, msg->src_port.len);
        uint8_t dst_port_len = msg->dst_port.len;
        net_send(sockfd, &dst_port_len, sizeof(uint8_t));
        net_send(sockfd, msg->dst_port.ptr, msg->dst_port.len);
        net_send(sockfd, (char *) &fixed_len, sizeof(uint32_t));
        net_send(sockfd, msg->buf, msg->len); 
        */

        net_disconnect(sockfd);
        return true;
    }

    return false;
}

bool node_recv_msg(int listenerfd, struct arena *arena, struct msg *msg) {
    int connfd = net_accept(listenerfd);
    if (connfd == -1)
        return false;

    uint32_t msg_len;
    if (!net_recv(connfd, (char *) &msg_len, sizeof(uint32_t))) {
        net_disconnect(connfd);
        return false;
    }

    size_t payload_size = msg_len - sizeof(uint32_t);
    char *buf = arena_malloc(arena, payload_size);

    if (!net_recv(connfd, buf, payload_size)) {
        net_disconnect(connfd);
        return true;
    }

    assert(msg_deserialize_resource(buf, msg));

    /*

    uint8_t fixed_type;
    if (!net_recv(connfd, (char *) &fixed_type, sizeof(uint8_t))) {
        net_disconnect(connfd);
        return false;
    }

    uint8_t src_port_len;
    if (!net_recv(connfd, (char *) &src_port_len, sizeof(uint8_t))) {
        net_disconnect(connfd);
        return false;
    }
    msg->src_port.len = src_port_len;
    msg->src_port.ptr = arena_malloc(arena, msg->src_port.len);
    if (!net_recv(connfd, msg->src_port.ptr, msg->src_port.len)) {
        net_disconnect(connfd);
        return false;
    }

    uint8_t dst_port_len;
    if (!net_recv(connfd, (char *) &dst_port_len, sizeof(uint8_t))) {
        net_disconnect(connfd);
        return false;
    }

    msg->dst_port.len = dst_port_len;
    msg->dst_port.ptr = arena_malloc(arena, msg->dst_port.len);
    if (!net_recv(connfd, msg->dst_port.ptr, msg->dst_port.len)) {
        net_disconnect(connfd);
        return false;
    }

    uint32_t fixed_len;
    if (!net_recv(connfd, (char *) &fixed_len, sizeof(uint32_t))) {
        net_disconnect(connfd);
        return false;
    }

    msg->type = (enum msg_type) fixed_type;
    msg->len = (size_t) fixed_len;
    msg->buf = arena_malloc(arena, msg->len);
    if (!net_recv(connfd, msg->buf, msg->len)) {
        net_disconnect(connfd);
        return false;
    }
    */

    net_disconnect(connfd);
    return true;
}

struct msg_queue {
    struct msg *values;
    int count;
    int capacity; 
    int front;
    struct arena arena;
    pthread_mutex_t mtx;
};

void msg_queue_init(struct msg_queue *queue) {
    arena_init(&queue->arena, malloc, realloc, free);
    queue->capacity = 8;
    queue->count = 0;
    queue->front = 0;
    queue->values = arena_malloc(&queue->arena, sizeof(struct msg) * queue->capacity);
    if (pthread_mutex_init(&queue->mtx, NULL)) {
        exit(1);
    }
}

void msg_queue_enqueue(struct msg_queue *queue, struct msg *msg) {
    pthread_mutex_lock(&queue->mtx);


    struct msg copy;
    msg_copy(msg, &queue->arena, &copy);

    if (queue->count == queue->capacity) {
        queue->capacity *= 2;
        queue->values = arena_realloc(&queue->arena, queue->values,  sizeof(struct msg) * queue->capacity);
    }

    queue->values[queue->count] = copy;
    queue->count++;

    pthread_mutex_unlock(&queue->mtx);
}

bool msg_queue_dequeue(struct msg_queue *queue, struct msg **msg) {
    pthread_mutex_lock(&queue->mtx);

    bool found = false;
    if (queue->front < queue->count) {
        *msg = &queue->values[queue->front];
        queue->front++;
        found = true;
    }

    pthread_mutex_unlock(&queue->mtx);
    return found;
}

void msg_queue_clear(struct msg_queue *queue) {
    pthread_mutex_lock(&queue->mtx);

    //only deallocate arena if all elements dequeued
    if (queue->count > 0 && queue->front == queue->count) {
        arena_dealloc_all(&queue->arena);
        queue->count = 0;
        queue->front = 0;
        queue->values = arena_malloc(&queue->arena, sizeof(struct msg) * queue->capacity);
    }

    pthread_mutex_unlock(&queue->mtx);
}

#endif //MSG_H
