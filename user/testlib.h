#ifndef TESTLIB_H
#define TESTLIB_H

#include "../userlib.h"
#include "../syscall.h"

static uint64_t __attribute__((unused)) t_get_tick(void) {
    register uint64_t a0 asm("a0");
    register uint64_t a7 asm("a7") = SYS_get_tick;
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return a0;
}

static void __attribute__((unused)) t_puts(const char *s) {
    write(1, s, strlen(s));
}

static void __attribute__((unused)) t_putu(uint64_t x) {
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

static int __attribute__((unused)) t_streq(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static void __attribute__((unused)) t_pass(const char *name) {
    t_puts("  [通过] ");
    t_puts(name);
    t_puts("\n");
}

static void __attribute__((unused)) t_fail(const char *name) {
    t_puts("  [失败] ");
    t_puts(name);
    t_puts("\n");
}

static void __attribute__((unused)) t_info(const char *name) {
    t_puts("  [提示] ");
    t_puts(name);
    t_puts("\n");
}

#endif
