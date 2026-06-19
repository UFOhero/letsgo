#include "userlib.h"

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    int fd;
    if (argc > 1) {
        fd = open(argv[1], O_RDONLY);
        if (fd < 0) {
            write(1, "cat: cannot open file\n", 22);
            exit(1);
        }
    } else {
        fd = 0;
    }
    char buf[64];
    int n;
    while ((n = read(fd, buf, 63)) > 0) {
        buf[n] = '\0';
        write(1, buf, n);
    }
    if (argc > 1) close(fd);
    exit(0);
}