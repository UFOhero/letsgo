#include "testlib.h"

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    t_puts("\n[TEST_EXEC] ELF loading, argv setup and exec\n");

    if (argc >= 3 && t_streq(argv[1], "child") && t_streq(argv[2], "argv-ok")) {
        t_pass("argv delivered to exec target");
        exit(33);
    }

    int pid = fork();
    if (pid < 0) {
        t_fail("fork before exec");
        exit(1);
    }

    if (pid == 0) {
        char *child_argv[] = {"test_exec", "child", "argv-ok", 0};
        exec("/bin/test_exec", child_argv);
        t_fail("exec returned unexpectedly");
        exit(2);
    }

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid && status == 33) {
        t_pass("ELF exec and child exit status");
    } else {
        t_fail("ELF exec and child exit status");
        t_puts("  status=");
        t_putu((uint64_t)status);
        t_puts("\n");
        exit(1);
    }

    exit(0);
}
