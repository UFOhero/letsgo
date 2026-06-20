#include "testlib.h"

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[TEST_MEM] Virtual memory, demand paging and fault isolation\n");

    volatile unsigned char *page = (volatile unsigned char *)0x60000000ULL;
    page[0] = 0x5a;
    page[4095] = 0xa5;
    if (page[0] == 0x5a && page[4095] == 0xa5) {
        t_pass("demand paging for user address");
    } else {
        t_fail("demand paging for user address");
        exit(1);
    }

    int pid = fork();
    if (pid < 0) {
        t_fail("fork child for page fault");
        exit(1);
    }
    if (pid == 0) {
        t_puts("  child writes NULL to trigger page fault\n");
        volatile int *bad = (volatile int *)0;
        *bad = 0x12345678;
        t_fail("NULL write should not return");
        exit(2);
    }

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid) {
        t_pass("faulting child was isolated and reaped");
    } else {
        t_fail("wait for faulting child");
        exit(1);
    }

    exit(0);
}
