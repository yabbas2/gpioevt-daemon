#include <string.h>
#include <stdlib.h>
#include "circular_buffer.h"

int circ_buf_init(circ_buf_t *buf, size_t size) {
    buf->entries = malloc(size * DATA_ENTRY_SIZE);
    if (!buf->entries) {
        return -1;
    }
    buf->max_size = size;
    buf->in = 0;
    buf->out = 0;
    buf->full = false;
    buf->empty = true;
    return 0;
}

void circ_buf_deinit(circ_buf_t *buf) {
    free(buf->entries);
    buf->entries = NULL;
    buf->max_size = 0;
    buf->in = 0;
    buf->out = 0;
    buf->full = false;
    buf->empty = true;
}

void circ_buf_push(circ_buf_t *buf, data_entry_t *entry) {
    if (buf->full) { // override if full
        buf->out = (buf->out + 1) % buf->max_size;
    }
    buf->entries[buf->in].deserialized = entry->deserialized;
    buf->in = (buf->in + 1) % buf->max_size;
    buf->empty = false;
    buf->full = (buf->in == buf->out);
}

data_entry_t *circ_buf_pop(circ_buf_t *buf) {
    if (buf->empty) {
        return NULL;
    }
    data_entry_t *entry = &buf->entries[buf->out];
    buf->out = (buf->out + 1) % buf->max_size;
    buf->full = false;
    buf->empty = (buf->in == buf->out);
    return entry;
}

bool circ_buf_is_empty(circ_buf_t *buf) { return buf->empty; }
bool circ_buf_is_full(circ_buf_t *buf) { return buf->full; }

size_t circ_buf_size(circ_buf_t *buf) {
    if (buf->full) {
        return buf->max_size;
    }
    if (buf->empty) {
        return 0;
    }
    if (buf->in > buf->out) {
        return buf->in - buf->out;
    } else {
        return buf->max_size - (buf->out - buf->in);
    }
}
