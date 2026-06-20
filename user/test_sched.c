#include "testlib.h"

#define CHILD_COUNT 3
#define ROUNDS 4

static void busy_work(int scale) {
    volatile int x = 0;
    for (int i = 0; i < scale; i++) {
        x += i;
    }
}

static void sched_yield_cpu(void) {
    register uint64_t a0 asm("a0");
    register uint64_t a7 asm("a7") = SYS_yield;
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
}

static void print_child_round(int child_id, int round) {
    t_puts("  子进程 ");
    t_putu((uint64_t)child_id);
    t_puts(" 获得 CPU，轮次 ");
    t_putu((uint64_t)round);
    t_puts("，tick=");
    t_putu(t_get_tick());
    t_puts("\n");
}

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[进程调度算法测试] 多进程/时间片调度展示\n");
    t_puts("  目标: 展示多个 RUNNABLE 子进程轮流获得 CPU，并最终由父进程 wait 回收\n");
    t_puts("  说明: 每轮打印当前 tick 后主动 yield，观察调度器如何选择下一个进程\n");

    int pids[CHILD_COUNT];
    for (int i = 0; i < CHILD_COUNT; i++) {
        int pid = fork();
        if (pid < 0) {
            t_fail("创建调度测试子进程");
            exit(1);
        }

        if (pid == 0) {
            for (int round = 0; round < ROUNDS; round++) {
                print_child_round(i, round);
                busy_work(80000 + i * 30000);
                sched_yield_cpu();
            }
            exit(40 + i);
        }

        pids[i] = pid;
    }

    int ok = 1;
    for (int i = 0; i < CHILD_COUNT; i++) {
        int status = 0;
        int ret = wait(pids[i], &status);
        if (ret != pids[i] || status != 40 + i) {
            ok = 0;
            t_puts("  回收异常: 子进程编号=");
            t_putu((uint64_t)i);
            t_puts(", wait返回=");
            t_putu((uint64_t)ret);
            t_puts(", 状态码=");
            t_putu((uint64_t)status);
            t_puts("\n");
        }
    }

    if (ok) {
        t_pass("多个子进程均获得 CPU，并被父进程正确回收");
    } else {
        t_fail("多个子进程均获得 CPU，并被父进程正确回收");
        exit(1);
    }

    t_puts("  [总结] 调度展示完成；观察输出顺序可看到进程间切换\n");
    exit(0);
}
