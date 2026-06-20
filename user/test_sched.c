#include "testlib.h"

#define CHILD_COUNT 3
#define YIELD_ROUNDS 2

struct sched_snapshot {
    int algorithm;
    int pid;
    int priority;
    uint64_t slice;
    int need_resched;
    uint64_t tick;
};

static void sched_yield_cpu(void) {
    register uint64_t a0 asm("a0");
    register uint64_t a7 asm("a7") = SYS_yield;
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
}

static void busy_work(int scale) {
    volatile int x = 0;
    for (int i = 0; i < scale; i++) {
        x += i;
    }
}

static const char *algorithm_name(int algorithm) {
    if (algorithm == 0) return "RR";
    if (algorithm == 1) return "FCFS";
    if (algorithm == 2) return "MLFQ";
    return "UNKNOWN";
}

static int read_sched_snapshot(struct sched_snapshot *s) {
    int ret = get_sched_info(&s->algorithm, &s->pid, &s->priority, &s->slice, &s->need_resched);
    s->tick = t_get_tick();
    return ret;
}

static void print_snapshot_line(const char *label, struct sched_snapshot *s) {
    t_puts("    ");
    t_puts(label);
    t_puts(": pid=");
    t_putu((uint64_t)s->pid);
    t_puts(", priority=");
    t_putu((uint64_t)s->priority);
    t_puts(", slice=");
    t_putu(s->slice);
    t_puts(", need_resched=");
    t_putu((uint64_t)s->need_resched);
    t_puts(", tick=");
    t_putu(s->tick);
    t_puts("\n");
}

static void run_time_slice_probe(void) {
    struct sched_snapshot before;
    struct sched_snapshot after;

    if (read_sched_snapshot(&before) < 0) {
        t_fail("读取时间片观察进程的调度状态");
        exit(2);
    }

    after = before;
    for (int i = 0; i < 8; i++) {
        busy_work(2500000);
        if (read_sched_snapshot(&after) < 0) {
            t_fail("读取时间片观察进程的调度状态");
            exit(2);
        }
        if (after.slice != before.slice ||
            after.priority != before.priority ||
            after.need_resched != before.need_resched) {
            break;
        }
    }

    t_puts("  [阶段1] 时间片计数观察\n");
    print_snapshot_line("忙等前", &before);
    print_snapshot_line("忙等后", &after);

    if (after.tick > before.tick &&
        (after.slice != before.slice || after.priority != before.priority || after.need_resched != before.need_resched)) {
        t_pass("忙等期间可观察到 slice/priority/need_resched 变化");
        exit(40);
    }

    t_fail("忙等期间可观察到 slice/priority/need_resched 变化");
    exit(3);
}

static void run_yield_child(int child_id) {
    for (int round = 0; round < YIELD_ROUNDS; round++) {
        struct sched_snapshot s;
        if (read_sched_snapshot(&s) < 0) {
            t_fail("读取主动 yield 进程的调度状态");
            exit(2);
        }

        t_puts("    子进程 ");
        t_putu((uint64_t)child_id);
        t_puts(" 第 ");
        t_putu((uint64_t)round);
        t_puts(" 轮运行: pid=");
        t_putu((uint64_t)s.pid);
        t_puts(", priority=");
        t_putu((uint64_t)s.priority);
        t_puts(", slice=");
        t_putu(s.slice);
        t_puts("\n");

        busy_work(800000 + child_id * 200000);
        sched_yield_cpu();
    }

    exit(50 + child_id);
}

static void test_time_slice_counter(void) {
    int pid = fork();
    if (pid < 0) {
        t_fail("创建时间片观察进程");
        exit(1);
    }

    if (pid == 0) {
        run_time_slice_probe();
    }

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid && status == 40) {
        t_pass("时间片观察进程被父进程正确回收");
    } else {
        t_fail("时间片观察进程被父进程正确回收");
        exit(1);
    }
}

static void test_cooperative_switch(void) {
    t_puts("\n  [阶段2] 多进程主动 yield 切换展示\n");

    int pids[CHILD_COUNT];
    for (int i = 0; i < CHILD_COUNT; i++) {
        int pid = fork();
        if (pid < 0) {
            t_fail("创建主动 yield 调度子进程");
            exit(1);
        }
        if (pid == 0) {
            run_yield_child(i);
        }
        pids[i] = pid;
    }

    int ok = 1;
    for (int i = 0; i < CHILD_COUNT; i++) {
        int status = 0;
        int ret = wait(pids[i], &status);
        if (ret != pids[i] || status != 50 + i) {
            ok = 0;
        }
    }

    if (ok) {
        t_pass("多个 RUNNABLE 子进程通过 yield 完成调度切换并被回收");
    } else {
        t_fail("多个 RUNNABLE 子进程通过 yield 完成调度切换并被回收");
        exit(1);
    }
}

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[进程调度算法测试] 时间片计数与主动调度展示\n");

    struct sched_snapshot info;
    if (read_sched_snapshot(&info) < 0) {
        t_fail("读取当前调度算法");
        exit(1);
    }

    t_puts("  当前调度算法=");
    t_puts(algorithm_name(info.algorithm));
    t_puts("，父进程pid=");
    t_putu((uint64_t)info.pid);
    t_puts("\n");
    t_puts("  说明: 本测试分两段输出，先看时间片计数，再看主动 yield 切换\n");
    t_puts("  说明: 当前内核不在 timer interrupt 中直接抢占切换\n\n");

    test_time_slice_counter();
    test_cooperative_switch();

    t_puts("\n  [总结] 调度展示完成：时间片计数可见，进程切换由 yield/wait/exit 安全触发\n");
    exit(0);
}
