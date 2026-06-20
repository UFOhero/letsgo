#include "testlib.h"

#define TRAP_TIMER      0
#define TRAP_UART       1
#define TRAP_SYSCALL    2
#define TRAP_PAGE_FAULT 3

static void require_ok(int cond, const char *name) {
    if (cond) {
        t_pass(name);
    } else {
        t_fail(name);
        exit(1);
    }
}

static uint64_t raw_unknown_syscall(void) {
    register uint64_t a0 asm("a0") = 0;
    register uint64_t a7 asm("a7") = 999;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

static void print_counter(const char *name, uint64_t value) {
    t_puts("    ");
    t_puts(name);
    t_puts("=");
    t_putu(value);
    t_puts("\n");
}

static void busy_wait_ticks(uint64_t min_delta) {
    uint64_t start = t_get_tick();
    while (t_get_tick() - start < min_delta) {
        yield_cpu();
    }
}

static void test_ecall_dispatch(void) {
    t_puts("\n  [用例1] ecall 异常入口与系统调用分发\n");

    uint64_t before = get_trap_count(TRAP_SYSCALL);
    const char *msg = "  用户态 write 通过 ecall 进入内核\n";
    int n = write(1, msg, strlen(msg));
    uint64_t after = get_trap_count(TRAP_SYSCALL);

    require_ok(n == (int)strlen(msg), "write 系统调用返回写入字节数");
    require_ok(after > before, "ecall trap 被内核统计到");
}

static void test_unknown_syscall(void) {
    t_puts("\n  [用例2] 未知系统调用的默认异常分支\n");

    uint64_t before = get_trap_count(TRAP_SYSCALL);
    uint64_t ret = raw_unknown_syscall();
    uint64_t after = get_trap_count(TRAP_SYSCALL);

    require_ok((int64_t)ret < 0, "未知 syscall 返回失败而不是破坏系统");
    require_ok(after > before, "未知 syscall 仍经过 ecall trap 分发");
}

static void test_timer_interrupt(void) {
    t_puts("\n  [用例3] S-mode 时钟中断与 tick 递增\n");

    uint64_t tick0 = t_get_tick();
    uint64_t timer0 = get_trap_count(TRAP_TIMER);

    busy_wait_ticks(250000);

    uint64_t tick1 = t_get_tick();
    uint64_t timer1 = get_trap_count(TRAP_TIMER);

    print_counter("tick_delta", tick1 - tick0);
    print_counter("timer_intr_delta", timer1 - timer0);

    require_ok(tick1 > tick0, "time 计数源持续递增");
    require_ok(timer1 > timer0, "时钟中断进入 trap_handler 并重新设置下一次中断");
}

static void test_demand_page_fault(void) {
    t_puts("\n  [用例4] 可恢复页故障：用户页按需映射\n");

    uint64_t before = get_trap_count(TRAP_PAGE_FAULT);
    volatile uint64_t *p = (volatile uint64_t *)0x62000000UL;
    *p = 0x1122334455667788ULL;
    uint64_t value = *p;
    uint64_t after = get_trap_count(TRAP_PAGE_FAULT);

    require_ok(value == 0x1122334455667788ULL, "页故障后新页可读写");
    require_ok(after > before, "页故障异常被 trap_handler 统计");
}

static void test_fault_isolation(void) {
    t_puts("\n  [用例5] 不可恢复异常隔离：空指针页故障杀死子进程\n");

    uint64_t before = get_trap_count(TRAP_PAGE_FAULT);
    int pid = fork();
    require_ok(pid >= 0, "fork 创建异常隔离测试子进程");

    if (pid == 0) {
        volatile uint64_t *bad = (volatile uint64_t *)0x0UL;
        *bad = 1;
        t_fail("空指针写入不应返回用户态继续执行");
        exit(2);
    }

    int status = 0;
    int ret = wait(pid, &status);
    uint64_t after = get_trap_count(TRAP_PAGE_FAULT);

    require_ok(ret == pid, "父进程可回收触发异常的子进程");
    require_ok(status < 0, "异常子进程以错误状态退出");
    require_ok(after > before, "空指针页故障被异常处理路径捕获");
}

static void test_uart_counter_hint(void) {
    t_puts("\n  [用例6] 外部中断计数展示：UART/PLIC\n");

    uint64_t uart = get_trap_count(TRAP_UART);
    print_counter("uart_intr_count", uart);
    t_puts("  [提示] UART 外部中断依赖实际键盘输入；自动测试只展示当前计数，不作为失败项\n");
}

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[中断与异常处理模块测试] trap/interrupt/exception 综合验证\n");
    t_puts("  覆盖指标: trap 向量、ecall、未知 syscall、时钟中断、页故障恢复、异常进程隔离、UART 外部中断计数\n");

    test_ecall_dispatch();
    test_unknown_syscall();
    test_timer_interrupt();
    test_demand_page_fault();
    test_fault_isolation();
    test_uart_counter_hint();

    t_puts("\n  [总结] 中断与异常处理相关检查完成\n");
    exit(0);
}
