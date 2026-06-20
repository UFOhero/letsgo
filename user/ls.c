#include "userlib.h"

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    const char *dir = ".";
    if (argc > 1)
        dir = argv[1];

    int fd = opendir(dir);
    if (fd < 0) {
        write(1, "ls: cannot open directory\n", 26);
        exit(1);
    }

    char name[28];
    while (readdir(fd, name) > 0) {
        write(1, name, strlen(name));
        write(1, "\n", 1);
    }
    close(fd);
    exit(0);
}