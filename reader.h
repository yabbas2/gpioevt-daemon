#ifndef READER_H
#define READER_H

#include <pthread.h>
#include "circular_buffer.h"

typedef struct {
    const char *device_path;
    circ_buf_t *buffer;
    pthread_mutex_t *mutex;
    int event_fd;
    volatile int *running;
} reader_ctx_t;

void *reader_thread(void *arg);

#endif
