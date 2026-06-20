#include "testlib.h"

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[TEST_INTR] Trap, syscall and timer source\n");

    uint64_t start = t_get_tick();
    uint64_t now = start;
    int guard = 0;
    while (now == start && guard < 1000000) {
        now = t_get_tick();
        guard++;
    }

    if (now > start) {
        t_pass("timer register is readable through syscall");
    } else {
        t_fail("timer register did not advance");
        exit(1);
    }

    if (write(1, "  external console output OK\n", 29) == 29) {
        t_pass("ecall trap dispatch");
    } else {
        t_fail("ecall trap dispatch");
        exit(1);
    }

    t_puts("  elapsed tick delta: ");
    t_putu(now - start);
    t_puts("\n");
    exit(0);
}
