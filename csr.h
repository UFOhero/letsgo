#ifndef _CSR_H
#define _CSR_H
#include <stdint.h>

// Supervisor Timer Interrupt Enable 位
#define SIE_STIE (1ULL << 5)  
// Supervisor External Interrupt Enable 位
#define SIE_SEIE (1ULL << 9)  
// Supervisor Interrupt Enable 位 (全局中断开关)
#define SSTATUS_SIE (1ULL << 1)

// 【新增】：用于状态切换的位掩码
#define SSTATUS_SPIE (1ULL << 5) // S 模式之前的硬件中断使能状态
#define SSTATUS_SPP  (1ULL << 8) // S 模式之前的特权级 (0=U-Mode, 1=S-Mode)
#define SSTATUS_SUM (1ULL << 18)

// 读取 scause (Trap原因)
static inline uint64_t r_scause() {
    uint64_t x;
    __asm__ volatile("csrr %0, scause" : "=r" (x) );
    return x;
}

// 读取 stval (通常包含缺页的虚拟地址)
static inline uint64_t r_stval() {
    uint64_t x;
    __asm__ volatile("csrr %0, stval" : "=r" (x) );
    return x;
}

// 读取 sepc (发生异常的指令地址)
static inline uint64_t r_sepc() {
    uint64_t x;
    __asm__ volatile("csrr %0, sepc" : "=r" (x) );
    return x;
}

// 写入 sepc
static inline void w_sepc(uint64_t x) {
    __asm__ volatile("csrw sepc, %0" : : "r" (x));
}

// 写入 stvec (陷阱向量基址)
static inline void w_stvec(uint64_t x) {
    __asm__ volatile("csrw stvec, %0" : : "r" (x));
}

// 读取 S-Mode 的中断使能寄存器
static inline uint64_t r_sie() {
    uint64_t x;
    asm volatile("csrr %0, sie" : "=r" (x) );
    return x;
}

static inline void w_sie(uint64_t x) {
    asm volatile("csrw sie, %0" : : "r" (x));
}

// 读取 S-Mode 的全局状态寄存器
static inline uint64_t r_sstatus() {
    uint64_t x;
    asm volatile("csrr %0, sstatus" : "=r" (x) );
    return x;
}

static inline void w_sstatus(uint64_t x) {
    asm volatile("csrw sstatus, %0" : : "r" (x));
}

// 读取当前时间 (硬件里的 mtime 会映射到 S 模式下的只读 time CSR)
static inline uint64_t r_time() {
    uint64_t x;
    asm volatile("rdtime %0" : "=r" (x) );
    return x;
}

#endif