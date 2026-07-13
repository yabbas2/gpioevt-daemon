#include <signal.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include <syslog.h>
#include <unistd.h>

#include "circular_buffer.h"
#include "reader.h"
#include "tcp_server.h"

#define UNUSED(x) (void)(x)
#define DEVICE_FILE "/dev/gpioevt"
#define BUF_SIZE 100
#define TCP_PORT 9090

static volatile sig_atomic_t running = 1;
static circ_buf_t buffer;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int event_fd;

static void sigint_handler(int sig) {
    UNUSED(sig);
    running = 0;
}

int main() {
    int ret;

#ifndef DEBUG
    ret = daemon(0, 0);
    if (ret != 0) {
        perror("daemon");
        return 1;
    }
    openlog("gpioevtd", LOG_PID, LOG_DAEMON);
#else
    printf("gpioevtd started in debug mode.\n");
    openlog("gpioevtd", LOG_PID, LOG_USER);
#endif

    struct sigaction sigint_action = {0};
    sigint_action.sa_handler = sigint_handler;
    sigfillset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);

    struct sigaction sigterm_action = {0};
    sigterm_action.sa_handler = sigint_handler;
    sigfillset(&sigterm_action.sa_mask);
    sigaction(SIGTERM, &sigterm_action, NULL);

    ret = circ_buf_init(&buffer, BUF_SIZE);
    if (ret != 0) {
        syslog(LOG_ERR, "main: failed to init circular buffer");
        return 1;
    }

    event_fd = eventfd(0, EFD_NONBLOCK);
    if (event_fd < 0) {
        syslog(LOG_ERR, "main: eventfd failed: %m");
        circ_buf_deinit(&buffer);
        return 1;
    }

    reader_ctx_t reader_ctx = {
        .device_path = DEVICE_FILE,
        .buffer = &buffer,
        .mutex = &mutex,
        .event_fd = event_fd,
        .running = &running,
    };

    tcp_server_ctx_t tcp_ctx = {
        .port = TCP_PORT,
        .event_fd = event_fd,
        .buffer = &buffer,
        .mutex = &mutex,
        .running = &running,
    };

    ret = tcp_server_init(&tcp_ctx);
    if (ret != 0) {
        syslog(LOG_ERR, "main: tcp_server_init failed");
        close(event_fd);
        circ_buf_deinit(&buffer);
        return 1;
    }

    pthread_t reader_tid, tcp_tid;

    ret = pthread_create(&reader_tid, NULL, reader_thread, &reader_ctx);
    if (ret != 0) {
        syslog(LOG_ERR, "main: pthread_create reader failed: %m");
        close(event_fd);
        circ_buf_deinit(&buffer);
        return 1;
    }

    ret = pthread_create(&tcp_tid, NULL, tcp_server_thread, &tcp_ctx);
    if (ret != 0) {
        syslog(LOG_ERR, "main: pthread_create tcp_server failed: %m");
        running = 0;
        pthread_join(reader_tid, NULL);
        close(event_fd);
        circ_buf_deinit(&buffer);
        return 1;
    }

    syslog(LOG_INFO, "main: gpioevtd started on port %d", TCP_PORT);

    while (running) {
        sleep(10);
    }

    syslog(LOG_INFO, "main: shutting down");

    pthread_join(reader_tid, NULL);
    pthread_join(tcp_tid, NULL);

    close(event_fd);
    circ_buf_deinit(&buffer);
    closelog();
    return 0;
}
