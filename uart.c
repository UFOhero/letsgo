#include <stdint.h>
#include <stdarg.h>
#include "string.h"   // 使用内核统一的 memcpy

volatile uint8_t * const UART = (uint8_t *)0x10000000;

void uart_init(void) {
    UART[3] = 0x03; 
    UART[2] = 0x07; 
    UART[1] = 0x01; 
}

void uart_putc(char c) {
    while ((UART[5] & 0x20) == 0) __asm__ volatile("nop");
    UART[0] = c;
}

char uart_getc(void) {
    if (UART[5] & 0x01) return UART[0];
    return 0;
}

void uart_puts(const char *str) { while (*str) uart_putc(*str++); }

//键盘环形缓冲区
#define KBD_BUF_SIZE 256
static char kbd_buf[KBD_BUF_SIZE];
static volatile int kbd_head = 0, kbd_tail = 0;

// 由硬件中断调用，将字符存入后台缓冲区
void uart_intr(void) {
    while (UART[5] & 0x01) {
        char c = UART[0];
        kbd_buf[kbd_tail] = c;
        kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    }
}

// 供 OS 和系统调用读取，如果没字符主动出让 CPU 等待，防止卡死
char uart_getc_blocking(void) {
    while (kbd_head == kbd_tail) {
        char c = uart_getc();
        if (c) return c;
        extern void yield(void);
        yield();
    }
    char c = kbd_buf[kbd_head];
    kbd_head = (kbd_head + 1) % KBD_BUF_SIZE;
    return c;
}

static void print_hex(uint64_t val) {
    const char *hex_chars = "0123456789abcdef";
    char buf[16];
    int i = 0;
    if (val == 0) { uart_putc('0'); return; }
    while (val > 0) {
        buf[i++] = hex_chars[val % 16];
        val /= 16;
    }
    while (--i >= 0) uart_putc(buf[i]);
}

static void print_dec(uint64_t val) {
    char buf[20];
    int i = 0;
    if (val == 0) { uart_putc('0'); return; }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (--i >= 0) uart_putc(buf[i]);
}

int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's') {
                uart_puts(va_arg(args, char *));
            } else if (*fmt == 'c') {
                uart_putc((char)va_arg(args, int));
            } else if (*fmt == 'l' && *(fmt + 1) == 'x') {
                fmt++;
                print_hex(va_arg(args, uint64_t));
            } else if (*fmt == 'l' && *(fmt + 1) == 'd') {
                fmt++;
                print_dec(va_arg(args, uint64_t));
            } else if (*fmt == 'd') {
                print_dec(va_arg(args, uint32_t));
            } else if (*fmt == 'x') {
                print_hex(va_arg(args, uint32_t));
            }
        } else {
            uart_putc(*fmt);
        }
        fmt++;
    }
    va_end(args);
    return 0;
}
