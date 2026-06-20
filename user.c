#include <stdint.h>

// 1. 封装底层 ecall 陷入指令
static inline uint64_t syscall(uint64_t id, uint64_t arg0) {
    register uint64_t a0 asm("a0") = arg0;
    register uint64_t a7 asm("a7") = id;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

// 2. 提供给用户的 printf 替代品
void print(const char *str) {
    syscall(1, (uint64_t)str); // SYS_write 的 ID 是 1
}

// 3. 软件延迟函数，防止刷屏
void delay(int count) {
    volatile int i = 0; // volatile 防止编译器把这个无聊的循环优化没掉
    while (i < count) i++;
}

// 4. 用户程序的真正入口
// __attribute__ 保证这个函数永远处于二进制文件的最顶端 (0x40000000)
__attribute__((section(".text.entry")))
void _start(int task_id) {
    // 内核在启动我们时，巧妙地把进程号塞在了 a0 (也就是参数 task_id) 里！
    while (1) {
        if (task_id == 0) {
            print("  --> Task 0 [Written in C!] is running gracefully...\n");
        } else {
            print("  --> Task 1 [Written in C!] is running gracefully...\n");
        }
        delay(5000000); // 停顿一下，让输出像瀑布一样优雅
    }
}