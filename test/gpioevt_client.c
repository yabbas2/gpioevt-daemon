#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 9090;

    struct hostent *he = gethostbyname(host);
    if (!he) {
        fprintf(stderr, "gethostbyname: %s\n", hstrerror(h_errno));
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    char buf[BUF_SIZE];
    size_t pos = 0;
    ssize_t n;

    while ((n = read(fd, buf + pos, sizeof(buf) - pos)) > 0) {
        pos += n;
        char *start = buf;
        char *nl;
        while ((nl = memchr(start, '\n', buf + pos - start))) {
            size_t len = nl - start + 1;
            fwrite(start, 1, len, stdout);
            fflush(stdout);
            start = nl + 1;
        }
        pos -= start - buf;
        memmove(buf, start, pos);
    }

    if (n < 0) {
        perror("read");
    }

    fprintf(stderr, "Connection closed.\n");
    close(fd);
    return 0;
}
