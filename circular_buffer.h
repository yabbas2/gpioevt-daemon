#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdbool.h>
#include "common.h"

#define CIRCULAR_BUFFER_DEFAULT_SIZE 100

typedef struct circ_buf {
    data_entry_t *entries;
    size_t max_size;
    size_t in;
    size_t out;
    bool full;
    bool empty;
} circ_buf_t;

int circ_buf_init(circ_buf_t *buf, size_t size);
void circ_buf_deinit(circ_buf_t *buf);
void circ_buf_push(circ_buf_t *buf, data_entry_t *entry);
data_entry_t *circ_buf_pop(circ_buf_t *buf);
bool circ_buf_is_empty(circ_buf_t *buf);
bool circ_buf_is_full(circ_buf_t *buf);
size_t circ_buf_size(circ_buf_t *buf);

#endif
