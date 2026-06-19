
#include <stdint.h>
#include "csr.h"
#include "sbi.h"

extern int printf(const char *fmt, ...);

// QEMU virt 机器的时钟频率是 10MHz (1秒跳动 10,000,000 次)
#define CLOCK_FREQ 100000

// 设定下一次中断发生在 1 秒后
void timer_set_next() {
    sbi_set_timer(r_time() + CLOCK_FREQ);
}

void timer_init() {
    // 1. 设置第一次闹钟
    timer_set_next();

    // 2. 开启 s 模式的定时器中断许可 (相当于打开了闹钟的独立开关)
    w_sie(r_sie() | SIE_STIE);

    // 3. 开启 s 模式全局中断 (相当于拉上了系统的总电闸)
    w_sstatus(r_sstatus() | SSTATUS_SIE);

    printf("[Timer] Initialized. Heartbeat starting...\n");
}