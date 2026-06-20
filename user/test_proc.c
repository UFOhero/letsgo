#include "testlib.h"

static volatile int parent_marker = 0x11;

static void busy_delay(int loops) {
    volatile int x = 0;
    for (int i = 0; i < loops; i++) {
        x += i;
    }
}

static void test_fork_return_and_exit(void) {
    t_puts("\n  [用例1] 进程创建：fork 返回值与父子分流\n");

    int pid = fork();
    if (pid < 0) {
        t_fail("fork 成功创建子进程");
        exit(1);
    }

    if (pid == 0) {
        t_puts("  子进程: fork 返回 0，进入子进程分支\n");
        exit(7);
    }

    t_pass("父进程获得子进程 PID");

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid) {
        t_pass("父进程 wait 返回正确的子进程 PID");
    } else {
        t_fail("父进程 wait 返回正确的子进程 PID");
        exit(1);
    }

    if (status == 7) {
        t_pass("子进程 exit 状态码正确传回父进程");
    } else {
        t_fail("子进程 exit 状态码正确传回父进程");
        t_puts("  实际状态码=");
        t_putu((uint64_t)status);
        t_puts("\n");
        exit(1);
    }
}

static void test_wait_block_and_wakeup(void) {
    t_puts("\n  [用例2] 状态转换：父进程 wait 阻塞，子进程 exit 唤醒\n");

    int pid = fork();
    if (pid < 0) {
        t_fail("创建用于 wait 阻塞测试的子进程");
        exit(1);
    }

    if (pid == 0) {
        t_puts("  子进程: 延迟一小段时间后退出\n");
        busy_delay(300000);
        exit(12);
    }

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid && status == 12) {
        t_pass("父进程从 BLOCKED 被子进程退出唤醒");
    } else {
        t_fail("父进程从 BLOCKED 被子进程退出唤醒");
        t_puts("  wait返回=");
        t_putu((uint64_t)ret);
        t_puts(", 状态码=");
        t_putu((uint64_t)status);
        t_puts("\n");
        exit(1);
    }
}

static void test_multiple_children(void) {
    t_puts("\n  [用例3] 多进程调度：连续创建并回收多个子进程\n");

    int pids[3];
    for (int i = 0; i < 3; i++) {
        int pid = fork();
        if (pid < 0) {
            t_fail("连续创建多个子进程");
            exit(1);
        }
        if (pid == 0) {
            t_puts("  子进程运行，编号=");
            t_putu((uint64_t)i);
            t_puts("\n");
            busy_delay(100000 * (i + 1));
            exit(20 + i);
        }
        pids[i] = pid;
    }

    int ok = 1;
    for (int i = 0; i < 3; i++) {
        int status = 0;
        int ret = wait(pids[i], &status);
        if (ret != pids[i] || status != 20 + i) {
            ok = 0;
            t_puts("  子进程回收异常: pid=");
            t_putu((uint64_t)pids[i]);
            t_puts(", wait返回=");
            t_putu((uint64_t)ret);
            t_puts(", 状态码=");
            t_putu((uint64_t)status);
            t_puts("\n");
        }
    }

    if (ok) {
        t_pass("多个 RUNNABLE/RUNNING 子进程均被调度并正确回收");
    } else {
        t_fail("多个 RUNNABLE/RUNNING 子进程均被调度并正确回收");
        exit(1);
    }
}

static void test_address_space_after_fork(void) {
    t_puts("\n  [用例4] 父子关系：fork 后父子进程拥有独立用户空间\n");

    parent_marker = 0x11;
    int pid = fork();
    if (pid < 0) {
        t_fail("创建用于父子空间隔离的子进程");
        exit(1);
    }

    if (pid == 0) {
        parent_marker = 0x55;
        if (parent_marker == 0x55) {
            t_pass("子进程可修改自己的数据副本");
            exit(31);
        }
        t_fail("子进程可修改自己的数据副本");
        exit(2);
    }

    int status = 0;
    if (wait(pid, &status) != pid) {
        t_fail("父进程可等待拥有父子关系的子进程");
        exit(1);
    }

    if (parent_marker == 0x11 && status == 31) {
        t_pass("父进程数据未被子进程修改，父子关系和回收正常");
    } else {
        t_fail("父进程数据未被子进程修改，父子关系和回收正常");
        t_puts("  parent_marker=");
        t_putu((uint64_t)parent_marker);
        t_puts(", 状态码=");
        t_putu((uint64_t)status);
        t_puts("\n");
        exit(1);
    }
}

static void test_wait_no_child(void) {
    t_puts("\n  [用例5] 边界情况：没有目标子进程时 wait 返回失败\n");

    int status = 0;
    int ret = wait(99, &status);
    if (ret < 0) {
        t_pass("wait 不会误回收不存在的子进程");
    } else {
        t_fail("wait 不会误回收不存在的子进程");
        t_puts("  wait返回=");
        t_putu((uint64_t)ret);
        t_puts("\n");
        exit(1);
    }
}

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[进程管理模块测试] 综合验证\n");
    t_puts("  覆盖指标: PCB状态转换、fork、wait、exit、父子关系、多进程调度、进程隔离\n");
    t_puts("  说明: 当前内核未提供 ps/kill 系统调用，因此通过用户态可观察行为间接验证 PCB 和调度器\n");

    test_fork_return_and_exit();
    test_wait_block_and_wakeup();
    test_multiple_children();
    test_address_space_after_fork();
    test_wait_no_child();

    t_puts("\n  [总结] 进程管理相关检查完成\n");
    exit(0);
}
