#include "testlib.h"

static volatile int image_marker = 0x1357;

static void require_ok(int cond, const char *name) {
    if (cond) {
        t_pass(name);
    } else {
        t_fail(name);
        exit(1);
    }
}

static void report_wait_error(int ret, int status) {
    t_puts("  wait返回=");
    t_putu((uint64_t)ret);
    t_puts(", 状态码=");
    t_putu((uint64_t)status);
    t_puts("\n");
}

static void child_mode_argv(int argc, char **argv) {
    int ok = argc == 4 &&
             t_streq(argv[0], "test_exec") &&
             t_streq(argv[1], "child-argv") &&
             t_streq(argv[2], "hello") &&
             t_streq(argv[3], "riscv64");

    if (ok) {
        t_pass("exec 目标程序收到正确 argc/argv");
        exit(33);
    }

    t_fail("exec 目标程序收到正确 argc/argv");
    t_puts("  argc=");
    t_putu((uint64_t)argc);
    t_puts("\n");
    exit(2);
}

static void child_mode_image(int argc, char **argv) {
    (void)argv;

    int ok = argc == 2 && image_marker == 0x1357;
    if (ok) {
        t_pass("exec 后用户数据段被重新加载");
        exit(34);
    }

    t_fail("exec 后用户数据段被重新加载");
    t_puts("  image_marker=");
    t_putu((uint64_t)image_marker);
    t_puts("\n");
    exit(3);
}

static void test_exec_self_with_argv(void) {
    t_puts("\n  [用例1] ELF 加载与 argc/argv 参数传递\n");

    int pid = fork();
    require_ok(pid >= 0, "fork 创建 exec 测试子进程");

    if (pid == 0) {
        char *child_argv[] = {"test_exec", "child-argv", "hello", "riscv64", 0};
        int ret = exec("/bin/test_exec", child_argv);
        (void)ret;
        t_fail("exec 成功时不应返回");
        exit(2);
    }

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid && status == 33) {
        t_pass("父进程 wait 回收 exec 后子进程退出码");
    } else {
        t_fail("父进程 wait 回收 exec 后子进程退出码");
        report_wait_error(ret, status);
        exit(1);
    }
}

static void test_exec_replaces_image(void) {
    t_puts("\n  [用例2] exec 替换进程映像并重置数据段\n");

    int pid = fork();
    require_ok(pid >= 0, "fork 创建映像替换测试子进程");

    if (pid == 0) {
        image_marker = 0x2468;
        char *child_argv[] = {"test_exec", "child-image", 0};
        int ret = exec("/bin/test_exec", child_argv);
        (void)ret;
        t_fail("exec 成功时不应返回到旧映像");
        exit(3);
    }

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid && status == 34) {
        t_pass("旧用户映像被新 ELF 替换");
    } else {
        t_fail("旧用户映像被新 ELF 替换");
        report_wait_error(ret, status);
        exit(1);
    }
}

static void test_exec_other_program(void) {
    t_puts("\n  [用例3] 加载并执行其他用户程序\n");

    int pid = fork();
    require_ok(pid >= 0, "fork 创建 /bin/null 执行子进程");

    if (pid == 0) {
        char *child_argv[] = {"null", "ignored-arg", 0};
        int ret = exec("/bin/null", child_argv);
        (void)ret;
        t_fail("exec /bin/null 成功时不应返回");
        exit(4);
    }

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid && status == 0) {
        t_pass("exec 可加载 /bin/null 并正常退出");
    } else {
        t_fail("exec 可加载 /bin/null 并正常退出");
        report_wait_error(ret, status);
        exit(1);
    }
}

static void test_exec_failure_keeps_process(void) {
    t_puts("\n  [用例4] exec 失败路径不会破坏当前进程\n");

    int pid = fork();
    require_ok(pid >= 0, "fork 创建 exec 失败测试子进程");

    if (pid == 0) {
        char *bad_argv[] = {"missing", 0};
        int ret = exec("/bin/not_exist_program", bad_argv);
        if (ret < 0) {
            t_pass("不存在的 ELF 返回失败");
            t_puts("  子进程: exec 失败后仍可继续执行并调用 exit\n");
            exit(35);
        }
        t_fail("不存在的 ELF 返回失败");
        exit(5);
    }

    int status = 0;
    int ret = wait(pid, &status);
    if (ret == pid && status == 35) {
        t_pass("exec 失败后进程状态和 wait 回收仍正常");
    } else {
        t_fail("exec 失败后进程状态和 wait 回收仍正常");
        report_wait_error(ret, status);
        exit(1);
    }
}

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    if (argc >= 2 && t_streq(argv[1], "child-argv")) {
        child_mode_argv(argc, argv);
    }

    if (argc >= 2 && t_streq(argv[1], "child-image")) {
        child_mode_image(argc, argv);
    }

    t_puts("\n[用户程序加载与执行模块测试] exec/ELF 综合验证\n");
    t_puts("  覆盖指标: ELF 读取与加载、进程映像替换、用户栈 argc/argv、失败返回、父进程 wait 回收\n");

    test_exec_self_with_argv();
    test_exec_replaces_image();
    test_exec_other_program();
    test_exec_failure_keeps_process();

    t_puts("\n  [总结] 用户程序加载与执行相关检查完成\n");
    exit(0);
}
