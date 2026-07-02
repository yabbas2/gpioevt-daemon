#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "circular_buffer.h"
#include "common.h"

#define UNUSED(x) (void)(x)
#define DEVICE_FILE "/dev/gpioevt"
#define LOG_SIZE 100

static volatile bool running = true;
static circ_buf_t log;

static void sigint_handler(int sig) {
    UNUSED(sig);
    running = false;
}

static void time_to_string(char *timebuf, size_t size, uint64_t ts) {
    snprintf(timebuf, size, "%llu", (unsigned long long)ts);
}

static void *reader_thread(void *arg) {
    UNUSED(arg);

    int fd = open(DEVICE_FILE, O_RDONLY, 0400);
    if (fd < 0) {
        perror("open");
        return NULL;
    }

    union data_entry data;

    while (running) {
        ssize_t n = read(fd, data.serialized, DATA_ENTRY_SIZE); // driver will not return partial data
        if (n < 0) {
            perror("read");
            break;
        }

        char timebuf[64];
        time_to_string(timebuf, sizeof(timebuf), data.deserialized.timestamp);
        syslog(LOG_INFO, "[%s] pin: %d, level: %d", timebuf, data.deserialized.pin, data.deserialized.level);
    }

    close(fd);
    return NULL;
}

int main() {
    int ret;
#ifndef DEBUG
    ret = daemon(0, 0); // convert to daemon
    if (ret != 0) {
        perror("daemon");
        return 1;
    }
    printf("gpioevtd started in daemon mode.\n");
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

    ret = circ_buf_init(&log, LOG_SIZE);
    if (ret != 0) {
        return 1;
    }

    pthread_t reader_tid;
    ret = pthread_create(&reader_tid, NULL, reader_thread, NULL);
    if (ret != 0) {
        perror("pthread_create");
        return 1;
    }

    while (running) {
        syslog(LOG_INFO, "gpioevtd running.");
        sleep(10);
    }

    printf("gpioevtd exiting.\n");
    pthread_join(reader_tid, NULL);
    circ_buf_deinit(&log);
    closelog();
    return 0;
}
