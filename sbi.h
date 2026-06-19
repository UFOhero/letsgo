
#ifndef _SBI_H
#define _SBI_H
#include <stdint.h>

struct sbiret {
    long error;
    long value;
};

// 发起 SBI 调用的底层函数
struct sbiret sbi_ecall(int ext, int fid, uint64_t arg0, uint64_t arg1, 
                        uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

// 设置下一次时钟中断
void sbi_set_timer(uint64_t stime_value);

#endif
