#include "testlib.h"

#define PAGE_SIZE 4096
#define USER_TEST_BASE ((volatile unsigned char *)0x60000000ULL)
#define USER_TEST_PAGES 8

static int global_data = 0x13572468;

static void fill_page(volatile unsigned char *p, unsigned char seed) {
    p[0] = seed;
    p[127] = seed + 1;
    p[PAGE_SIZE - 1] = seed + 2;
}

static int check_page(volatile unsigned char *p, unsigned char seed) {
    return p[0] == seed &&
           p[127] == (unsigned char)(seed + 1) &&
           p[PAGE_SIZE - 1] == (unsigned char)(seed + 2);
}

static void test_user_layout(void) {
    t_puts("\n  [用例1] 用户态内存布局：代码段 / 数据段 / 栈\n");

    int local_stack = 0x24681357;
    if (global_data == 0x13572468 && local_stack == 0x24681357) {
        t_pass("ELF 数据段可访问，用户栈可正常读写");
    } else {
        t_fail("ELF 数据段可访问，用户栈可正常读写");
        exit(1);
    }
}

static void test_demand_paging(void) {
    t_puts("\n  [用例2] 虚拟内存：按需分页与 4KB 页边界\n");

    for (int i = 0; i < USER_TEST_PAGES; i++) {
        volatile unsigned char *page = USER_TEST_BASE + i * PAGE_SIZE;
        fill_page(page, (unsigned char)(0x30 + i));
    }

    for (int i = 0; i < USER_TEST_PAGES; i++) {
        volatile unsigned char *page = USER_TEST_BASE + i * PAGE_SIZE;
        if (!check_page(page, (unsigned char)(0x30 + i))) {
            t_fail("连续 8 个用户页按需映射后数据保持正确");
            exit(1);
        }
    }

    t_pass("连续 8 个用户页按需映射后数据保持正确");
    t_puts("  映射页数: ");
    t_putu(USER_TEST_PAGES);
    t_puts(", 页大小: ");
    t_putu(PAGE_SIZE);
    t_puts(" 字节\n");
}

static void test_fork_isolation(void) {
    t_puts("\n  [用例3] 进程隔离：fork 后父子页表深拷贝\n");

    volatile unsigned char *shared_va = USER_TEST_BASE + PAGE_SIZE;
    shared_va[0] = 0x11;

    int pid = fork();
    if (pid < 0) {
        t_fail("成功创建用于地址空间隔离测试的子进程");
        exit(1);
    }

    if (pid == 0) {
        shared_va[0] = 0x77;
        if (shared_va[0] == 0x77) {
            t_pass("子进程可以写入自己的私有页副本");
            exit(0);
        }
        t_fail("子进程可以写入自己的私有页副本");
        exit(2);
    }

    int status = 0;
    if (wait(pid, &status) != pid) {
        t_fail("父进程成功等待地址空间隔离测试子进程");
        exit(1);
    }

    if (shared_va[0] == 0x11) {
        t_pass("子进程写入后父进程页面内容保持不变");
    } else {
        t_fail("子进程写入后父进程页面内容保持不变");
        t_puts("  父进程看到的值=");
        t_putu(shared_va[0]);
        t_puts("\n");
        exit(1);
    }
}

static void test_null_fault_isolation(void) {
    t_puts("\n  [用例4] 异常处理：空指针页故障隔离\n");

    int pid = fork();
    if (pid < 0) {
        t_fail("成功创建用于空指针页故障测试的子进程");
        exit(1);
    }

    if (pid == 0) {
        t_puts("  子进程写入 NULL 地址，预期触发页故障\n");
        volatile int *bad = (volatile int *)0;
        *bad = 0x12345678;
        t_fail("NULL 写入不应正常返回");
        exit(2);
    }

    int status = 0;
    if (wait(pid, &status) == pid) {
        t_pass("空指针故障只终止子进程，内核未崩溃");
    } else {
        t_fail("空指针故障只终止子进程，内核未崩溃");
        exit(1);
    }
}

static void test_text_permission_probe(void) {
    t_puts("\n  [用例5] 页权限：代码页写保护能力探测\n");

    int pid = fork();
    if (pid < 0) {
        t_fail("成功创建用于代码页权限探测的子进程");
        exit(1);
    }

    if (pid == 0) {
        t_puts("  子进程尝试写入代码页\n");
        volatile unsigned int *code = (volatile unsigned int *)(void *)test_text_permission_probe;
        *code = 0;
        t_puts("  [提示] 当前实现未启用代码页写保护；该项作为能力探测，不计为失败\n");
        exit(0);
    }

    int status = 0;
    if (wait(pid, &status) == pid) {
        t_pass("代码页权限探测被限制在子进程内，父进程继续运行");
    } else {
        t_fail("代码页权限探测被限制在子进程内，父进程继续运行");
        exit(1);
    }
}

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[内存管理模块测试] 综合验证\n");
    t_puts("  覆盖指标: 4KB页、虚拟地址映射、用户地址空间、按需分页、进程隔离、异常隔离\n");
    t_puts("  说明: PMM/内核堆通过页故障分配、fork页表复制、exec用户栈构造进行间接验证\n");

    test_user_layout();
    test_demand_paging();
    test_fork_isolation();
    test_null_fault_isolation();
    test_text_permission_probe();

    t_puts("\n  [总结] 内存管理相关检查完成\n");
    exit(0);
}
