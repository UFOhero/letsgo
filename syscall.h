#ifndef _SYSCALL_H
#define _SYSCALL_H
#include <stdint.h>

// 【核心重构】：让结构体与汇编上下文完美对应，不留任何歧义
struct trapframe {
    /* 0 */ uint64_t ra;
    /* 8 */ uint64_t sp;
    /* 16 */ uint64_t gp;
    /* 24 */ uint64_t tp;
    /* 32 */ uint64_t t0;
    /* 40 */ uint64_t t1;
    /* 48 */ uint64_t t2;
    /* 56 */ uint64_t s0;
    /* 64 */ uint64_t s1;
    /* 72 */ uint64_t a0;
    /* 80 */ uint64_t a1;
    /* 88 */ uint64_t a2;
    /* 96 */ uint64_t a3;
    /* 104 */ uint64_t a4;
    /* 112 */ uint64_t a5;
    /* 120 */ uint64_t a6;
    /* 128 */ uint64_t a7; // a7 在偏移量 128 (16 * 8)
    /* 136 */ uint64_t s2;
    /* 144 */ uint64_t s3;
    /* 152 */ uint64_t s4;
    /* 160 */ uint64_t s5;
    /* 168 */ uint64_t s6;
    /* 176 */ uint64_t s7;
    /* 184 */ uint64_t s8;
    /* 192 */ uint64_t s9;
    /* 200 */ uint64_t s10;
    /* 208 */ uint64_t s11;
    /* 216 */ uint64_t t3;
    /* 224 */ uint64_t t4;
    /* 232 */ uint64_t t5;
    /* 240 */ uint64_t t6;
    /* 248 */ uint64_t sepc;
    /* 256 */ uint64_t sstatus;
};


#define SYS_print     1
#define SYS_write     2
#define SYS_read      3
#define SYS_open      4
#define SYS_close     5
#define SYS_opendir   6
#define SYS_readdir   7
#define SYS_exec      8
#define SYS_exit      9
#define SYS_dup2      10
#define SYS_fork      11
#define SYS_wait      12
#define SYS_get_tick  13
#define SYS_ps        14
#define SYS_kill      15

void handle_syscall(struct trapframe *tf);
#endif