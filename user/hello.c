#include "userlib.h"

static void puts1(const char *s) {
    write(1, s, strlen(s));
}

static void putint(int x) {
    char buf[16];
    int i = 0;
    if (x == 0) {
        write(1, "0", 1);
        return;
    }
    if (x < 0) {
        write(1, "-", 1);
        x = -x;
    }
    while (x > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + x % 10);
        x /= 10;
    }
    while (i > 0) {
        char c = buf[--i];
        write(1, &c, 1);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int pid = fork();
    if (pid < 0) {
        puts1("spawn: fork failed\n");
        return 1;
    }

    if (pid == 0) {
        while (1) {
            yield_cpu();
        }
    }

    puts1("spawn: child pid = ");
    putint(pid);
    puts1("\n");
    return 0;
}