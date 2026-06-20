#include "testlib.h"

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[TEST_PROC] Process creation, wait and exit status\n");

    int pid = fork();
    if (pid < 0) {
        t_fail("fork");
        exit(1);
    }
    if (pid == 0) {
        t_puts("  child process is running\n");
        exit(7);
    }

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid) {
        t_pass("parent waited child pid");
    } else {
        t_fail("parent waited child pid");
        exit(1);
    }

    if (status == 7) {
        t_pass("exit status propagated");
    } else {
        t_fail("exit status propagated");
        t_puts("  got status=");
        t_putu((uint64_t)status);
        t_puts("\n");
        exit(1);
    }

    exit(0);
}
