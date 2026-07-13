#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "tcp_server.h"

#define MAX_EVENTS 64
#define MAX_CLIENTS 128
#define LINE_BUF_SIZE 128

static int client_fds[MAX_CLIENTS];
static size_t num_clients = 0;

static void remove_client(size_t index) {
    close(client_fds[index]);
    client_fds[index] = client_fds[--num_clients];
}

static void broadcast_event(data_entry_t *entry) {
    time_t ts = (time_t)entry->deserialized.timestamp;
    struct tm tm;
    char timebuf[64];
    char line[LINE_BUF_SIZE];

    localtime_r(&ts, &tm);
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &tm);
    int len = snprintf(line, sizeof(line), "TS=%s PIN=%d LVL=%d\n",
                       timebuf, entry->deserialized.pin, entry->deserialized.level);

    size_t i = 0;
    while (i < num_clients) {
        ssize_t ret = send(client_fds[i], line, (size_t)len, MSG_DONTWAIT);
        if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            i++;
        } else if (ret < 0 || ret == 0) {
            syslog(LOG_WARNING, "tcp_server: removing client fd %d", client_fds[i]);
            remove_client(i);
        } else {
            i++;
        }
    }
}

int tcp_server_init(tcp_server_ctx_t *ctx) {
    ctx->listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (ctx->listen_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(ctx->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)ctx->port),
        .sin_addr = {.s_addr = htonl(INADDR_ANY)},
    };

    if (bind(ctx->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(ctx->listen_fd);
        return -1;
    }

    if (listen(ctx->listen_fd, 8) < 0) {
        perror("listen");
        close(ctx->listen_fd);
        return -1;
    }

    ctx->epoll_fd = epoll_create1(0);
    if (ctx->epoll_fd < 0) {
        perror("epoll_create1");
        close(ctx->listen_fd);
        return -1;
    }

    struct epoll_event ev = {.events = EPOLLIN, .data = {.fd = ctx->listen_fd}};
    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->listen_fd, &ev) < 0) {
        perror("epoll_ctl listen");
        close(ctx->listen_fd);
        close(ctx->epoll_fd);
        return -1;
    }

    ev.data.fd = ctx->event_fd;
    if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, ctx->event_fd, &ev) < 0) {
        perror("epoll_ctl eventfd");
        close(ctx->listen_fd);
        close(ctx->epoll_fd);
        return -1;
    }

    return 0;
}

void *tcp_server_thread(void *arg) {
    tcp_server_ctx_t *ctx = (tcp_server_ctx_t *)arg;
    struct epoll_event events[MAX_EVENTS];
    uint64_t notification;

    syslog(LOG_INFO, "tcp_server: listening on port %d", ctx->port);

    while (*ctx->running) {
        int nfds = epoll_wait(ctx->epoll_fd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == ctx->listen_fd) {
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);
                int client_fd = accept(ctx->listen_fd,
                                        (struct sockaddr *)&client_addr,
                                        &addrlen);
                if (client_fd >= 0) {
                    fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL) | O_NONBLOCK);
                }
                if (client_fd < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("accept4");
                    }
                    continue;
                }

                if (num_clients >= MAX_CLIENTS) {
                    syslog(LOG_WARNING, "tcp_server: max clients reached, rejecting");
                    close(client_fd);
                    continue;
                }

                struct epoll_event cev = {.events = EPOLLIN | EPOLLRDHUP,
                                          .data = {.fd = client_fd}};
                if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, client_fd, &cev) < 0) {
                    perror("epoll_ctl client");
                    close(client_fd);
                    continue;
                }

                client_fds[num_clients++] = client_fd;
                syslog(LOG_INFO, "tcp_server: client connected (total: %zu)", num_clients);

            } else if (fd == ctx->event_fd) {
                read(ctx->event_fd, &notification, sizeof(notification));

                pthread_mutex_lock(ctx->mutex);
                while (!circ_buf_is_empty(ctx->buffer)) {
                    data_entry_t *entry = circ_buf_pop(ctx->buffer);
                    broadcast_event(entry);
                }
                pthread_mutex_unlock(ctx->mutex);

            } else {
                close(fd);
                for (size_t j = 0; j < num_clients; j++) {
                    if (client_fds[j] == fd) {
                        remove_client(j);
                        syslog(LOG_INFO, "tcp_server: client disconnected (total: %zu)", num_clients);
                        break;
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < num_clients; i++) {
        close(client_fds[i]);
    }
    num_clients = 0;

    close(ctx->listen_fd);
    close(ctx->epoll_fd);

    syslog(LOG_INFO, "tcp_server: shutting down");
    return NULL;
}
