#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "reader.h"

#define UNUSED(x) (void)(x)

void *reader_thread(void *arg) {
    reader_ctx_t *ctx = (reader_ctx_t *)arg;

    int fd = open(ctx->device_path, O_RDONLY, 0400);
    if (fd < 0) {
        syslog(LOG_ERR, "reader: failed to open %s: %m", ctx->device_path);
        return NULL;
    }

    struct pollfd fds = {.fd = fd, .events = POLLIN, .revents = 0};
    union data_entry data;
    uint64_t notification = 1;

    while (*ctx->running) {
        int ret = poll(&fds, 1, 1000);
        if (ret < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "reader: poll failed: %m");
            break;
        }
        if (ret == 0) continue;

        ssize_t n = read(fd, data.serialized, DATA_ENTRY_SIZE);
        if (n < 0) {
            syslog(LOG_ERR, "reader: read failed: %m");
            break;
        }

        pthread_mutex_lock(ctx->mutex);
        circ_buf_push(ctx->buffer, &data);
        write(ctx->event_fd, &notification, sizeof(notification));
        pthread_mutex_unlock(ctx->mutex);

        syslog(LOG_INFO, "reader: pin=%d level=%d", data.deserialized.pin, data.deserialized.level);
    }

    close(fd);
    syslog(LOG_INFO, "reader: shutting down");
    return NULL;
}
