#ifndef TESTLIB_H
#define TESTLIB_H

#include "../userlib.h"
#include "../syscall.h"

static uint64_t t_get_tick(void) {
    register uint64_t a0 asm("a0");
    register uint64_t a7 asm("a7") = SYS_get_tick;
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return a0;
}

static void t_puts(const char *s) {
    write(1, s, strlen(s));
}

static void t_putu(uint64_t x) {
    char buf[32];
    int i = 0;
    if (x == 0) {
        write(1, "0", 1);
        return;
    }
    while (x > 0 && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (x % 10);
        x /= 10;
    }
    while (i > 0) {
        i--;
        write(1, &buf[i], 1);
    }
}

static int t_streq(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static void t_pass(const char *name) {
    t_puts("  [PASS] ");
    t_puts(name);
    t_puts("\n");
}

static void t_fail(const char *name) {
    t_puts("  [FAIL] ");
    t_puts(name);
    t_puts("\n");
}

#endif
