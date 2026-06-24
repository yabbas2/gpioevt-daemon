#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)

static bool running = true;

static void sigint_handler(int sig) {
    UNUSED(sig);
    running = false;
}

int main() {
#ifndef DEBUG
    int ret = daemon(0, 0); // convert to daemon
    if (ret != 0) {
        perror("daemon");
        return 1;
    }
    printf("gpioevtd started in daemon mode.\n");
#else
    printf("gpioevtd started in debug mode.\n");
#endif
    openlog("gpioevtd", LOG_PID, LOG_DAEMON);

    struct sigaction sigint_action;
    sigint_action.sa_handler = sigint_handler;
    sigemptyset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);

    struct sigaction sigterm_action;
    sigterm_action.sa_handler = sigint_handler;
    sigemptyset(&sigterm_action.sa_mask);
    sigaction(SIGTERM, &sigterm_action, NULL);

    while (running) {
        syslog(LOG_INFO, "gpioevtd running.");
        sleep(10);
    }

    printf("gpioevtd exiting.\n");
    closelog();
    return 0;
}
