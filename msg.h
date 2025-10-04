#ifndef MSG_H
#define MSG_H

#include <pthread.h>
#include "arena.h"

enum msg_type {
    MT_DGREP,
    MT_JOIN_REQ,
    MT_JOIN_RES
};

struct msg {
    enum msg_type type;
    int sockfd; //TODO: get rid of this since (listening) ports will be used as ids to connect
    char* buf;
    size_t len;
    char src_port[4];
    char dst_port[4];
};


void node_send_msg(struct msg *msg) {
    uint8_t fixed_type = (uint8_t) msg->type;
    uint32_t fixed_len = (uint32_t) msg->len;
    net_send(msg->sockfd, (char *) &fixed_type, sizeof(uint8_t));
    net_send(msg->sockfd, msg->src_port, 4);
    net_send(msg->sockfd, msg->dst_port, 4);
    net_send(msg->sockfd, (char *) &fixed_len, sizeof(uint32_t));
    net_send(msg->sockfd, msg->buf, msg->len); 
}

bool node_recv_msg(struct msg *msg, struct arena *arena) {
    uint8_t fixed_type;
    if (!net_recv(msg->sockfd, (char *) &fixed_type, sizeof(uint8_t))) {
        return false;
    }

    if (!net_recv(msg->sockfd, msg->src_port, 4)) {
        return false;
    }

    if (!net_recv(msg->sockfd, msg->dst_port, 4)) {
        return false;
    }

    uint32_t fixed_len;
    if (!net_recv(msg->sockfd, (char *) &fixed_len, sizeof(uint32_t))) {
        return false;
    }

    msg->type = (enum msg_type) fixed_type;
    msg->len = (size_t) fixed_len;
    msg->buf = arena_malloc(arena, msg->len);
    if (!net_recv(msg->sockfd, msg->buf, msg->len)) {
        free(msg->buf);
        return false;
    }

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

    struct msg copy = *msg;
    copy.buf = arena_malloc(&queue->arena, copy.len);
    memcpy(copy.buf, msg->buf, copy.len);

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

    arena_dealloc_all(&queue->arena);
    queue->count = 0;
    queue->front = 0;
    queue->values = arena_malloc(&queue->arena, sizeof(struct msg) * queue->capacity);

    pthread_mutex_unlock(&queue->mtx);
}

#endif //MSG_H
