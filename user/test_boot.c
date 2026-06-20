#include "testlib.h"

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[TEST_BOOT] System startup and user entry\n");
    t_puts("  If this program runs, OpenSBI -> kernel -> shell -> exec chain is alive.\n");

    if (write(1, "  console write syscall OK\n", 27) == 27) {
        t_pass("user-mode write syscall");
    } else {
        t_fail("user-mode write syscall");
        exit(1);
    }

    uint64_t t0 = t_get_tick();
    uint64_t t1 = t_get_tick();
    t_puts("  tick sample: ");
    t_putu(t0);
    t_puts(" -> ");
    t_putu(t1);
    t_puts("\n");
    t_pass("kernel booted and user program loaded");
    exit(0);
}
