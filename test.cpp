//
// Created by dalaoshe on 17-7-17.
//
#include <cstdio>

#include "unp.h"

int main(int argc, char** argv) {
    fprintf(stderr, "\n\nOpen fifo pipe write\n\n");
    int fd = mkfifo(argv[1], O_WRONLY);
    if(fd < 0) {
        fprintf(stderr, "\n\nCreate fifo pipe error %s\n\n", strerror(errno));
        fd = open(argv[1], O_WRONLY);
        if(fd < 0) {
            fprintf(stderr, "\n\nOpen fifo pipe error %s\n\n", strerror(errno));
        }
    }
    fprintf(stderr, "\n\nOpen fifo pipe SUCCESS\n\n");

    char buf[1024];
    int n;
    while (1) {
        scanf("%s", buf);
        n = sizeof(buf);
        fprintf(stderr, "\n\nRead %d byte from pipe, content:%s\n\n", n, buf);
        int s = write(fd, buf, n);
        fprintf(stderr, "\n\nWrite %d byte to pipe, content:%s error:%s\n\n", s, buf, strerror(errno));
    }
}