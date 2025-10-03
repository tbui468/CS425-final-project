#ifndef MSG_H
#define MSG_H

enum msg_type {
    MT_DGREP,
    MT_JOIN_REQ,
    MT_JOIN_RES
};

struct msg {
    enum msg_type type;
    int sockfd;
    char* buf;
    size_t len;
};


void node_send_msg(struct msg *msg) {
    uint8_t fixed_type = (uint8_t) msg->type;
    uint32_t fixed_len = (uint32_t) msg->len;
    net_send(msg->sockfd, (char *) &fixed_type, sizeof(uint8_t));
    net_send(msg->sockfd, (char *) &fixed_len, sizeof(uint32_t));
    net_send(msg->sockfd, msg->buf, msg->len); 
}

bool node_recv_msg(struct msg *msg) {
    uint8_t fixed_type;
    if (!net_recv(msg->sockfd, (char *) &fixed_type, sizeof(uint8_t))) {
        return false;
    }

    uint32_t fixed_len;
    if (!net_recv(msg->sockfd, (char *) &fixed_len, sizeof(uint32_t))) {
        return false;
    }

    msg->type = (enum msg_type) fixed_type;
    msg->len = (size_t) fixed_len;
    msg->buf = malloc(msg->len);
    if (!net_recv(msg->sockfd, msg->buf, msg->len)) {
        free(msg->buf);
        return false;
    }

    return true;
}


#endif //MSG_H
