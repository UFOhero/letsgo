#include "sbi.h"

struct sbiret sbi_ecall(int ext, int fid, uint64_t arg0, uint64_t arg1, 
                        uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    struct sbiret ret;
    register uint64_t a0 asm ("a0") = (uint64_t)(arg0);
    register uint64_t a1 asm ("a1") = (uint64_t)(arg1);
    register uint64_t a2 asm ("a2") = (uint64_t)(arg2);
    register uint64_t a3 asm ("a3") = (uint64_t)(arg3);
    register uint64_t a4 asm ("a4") = (uint64_t)(arg4);
    register uint64_t a5 asm ("a5") = (uint64_t)(arg5);
    register uint64_t a6 asm ("a6") = (uint64_t)(fid);
    register uint64_t a7 asm ("a7") = (uint64_t)(ext);

    // 触发 ecall，陷入 M-Mode 交由 OpenSBI 处理
    asm volatile ("ecall"
                  : "+r" (a0), "=r" (a1)
                  : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
                  : "memory");
    ret.error = a0;
    ret.value = a1;
    return ret;
}

void sbi_set_timer(uint64_t stime_value) {
    // 0x54494D45 是 TIME 扩展的标准 EID (恰好是 'TIME' 的 ASCII 码)
    sbi_ecall(0x54494D45, 0, stime_value, 0, 0, 0, 0, 0);
}