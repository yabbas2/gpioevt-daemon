#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <pthread.h>
#include "circular_buffer.h"

typedef struct {
    int port;
    int listen_fd;
    int epoll_fd;
    int event_fd;
    circ_buf_t *buffer;
    pthread_mutex_t *mutex;
    volatile int *running;
} tcp_server_ctx_t;

int tcp_server_init(tcp_server_ctx_t *ctx);
void *tcp_server_thread(void *arg);

#endif
