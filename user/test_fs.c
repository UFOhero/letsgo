#include "testlib.h"

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[TEST_FS] RAMFS file, directory and fd operations\n");

    const char *path = "/tmp/fs_test.txt";
    const char *msg = "file-system-test";
    int fd = open(path, O_CREAT | O_RDWR | O_TRUNC);
    if (fd < 0) {
        t_fail("create file");
        exit(1);
    }

    int n = write(fd, msg, strlen(msg));
    close(fd);
    if (n == (int)strlen(msg)) {
        t_pass("write file content");
    } else {
        t_fail("write file content");
        exit(1);
    }

    char buf[64];
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        t_fail("open file for read");
        exit(1);
    }
    n = read(fd, buf, 63);
    close(fd);
    if (n >= 0) buf[n] = 0;
    if (n == (int)strlen(msg) && t_streq(buf, msg)) {
        t_pass("read file content");
    } else {
        t_fail("read file content");
        exit(1);
    }

    int dir = opendir("/tmp");
    if (dir < 0) {
        t_fail("open /tmp directory");
        exit(1);
    }
    int found = 0;
    char name[32];
    while (readdir(dir, name) > 0) {
        if (t_streq(name, "fs_test.txt")) found = 1;
    }
    close(dir);
    if (found) {
        t_pass("directory traversal finds file");
    } else {
        t_fail("directory traversal finds file");
        exit(1);
    }

    exit(0);
}
