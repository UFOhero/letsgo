#include "syscall.h"
#include "csr.h"
#include "proc.h"
#include "fs.h"
#include "elf.h"
#include "string.h"
#include "vmm.h"

extern int printf(const char *fmt, ...);
extern volatile int switch_to_user;
extern uint64_t user_satp;

static uint64_t sys_write(uint64_t fd, const char *buf, uint64_t count) {
    // 1. 让 VFS (虚拟文件系统) 接管真正的写入逻辑！
    // 如果你在 Shell 里执行了 '>'，此时 fd 1 已经指向了文件，这里就会把数据写入文件。
    // 如果 fd 1 正常指向 TERMINAL_INODE，fs_write 自己也会调用 uart_putc 打印到屏幕。
    int ret = fs_write(fd, buf, count);
    
    // 如果 VFS 成功处理了（返回值 >= 0），任务完成！
    if (ret >= 0) {
        return ret; 
    }

    // 2. 终极兼容性兜底 (Fallback)
    // 为什么要有这步？因为你的系统在最开始可能没有显式把 fd 1 和 fd 2 初始化为 TERMINAL_INODE。
    // 如果 fs_write 因为 fd 表没初始化而报错 (-1)，
    // 并且目标是标准输出 (1) 或标准错误 (2)，我们强行将其路由到屏幕，保证系统不会变哑巴。
    if (fd == 1 || fd == 2) {
        for (uint64_t i = 0; i < count; i++) {
            char c = ((char*)buf)[i];
            extern void uart_putc(char c);
            uart_putc(c);
        }
        return count;
    }

    return -1;
}

static uint64_t sys_read(uint64_t fd, char *buf, uint64_t count) {
    if (fd == 0) {
        extern char uart_getc_blocking(void);
        if (count > 0) {
            buf[0] = uart_getc_blocking();
            return 1;
        }
        return 0;
    }
    return fs_read(fd, buf, count);
}

static uint64_t sys_open(const char *path, uint64_t flags) {
    return fs_open(path, flags);
}

static uint64_t sys_close(uint64_t fd) {
    return fs_close(fd);
}

static uint64_t sys_opendir(const char *path) {
    printf("[sys_opendir] path=%s, addr=0x%lx\n", path, (uint64_t)path);
    int ret = fs_opendir(path);
    printf("[sys_opendir] returns %d\n", ret);
    return ret;
}

static uint64_t sys_readdir(uint64_t fd, char *name) {
    return fs_readdir(fd, name);
}

static uint64_t sys_exec(const char *path, char *const argv[], struct trapframe *tf) {
    int ret = exec(path, argv, NULL, NULL);
    if (ret == 0) {
        // 设置 trapframe 以返回用户态
        tf->sepc = current->entry;
        tf->sstatus = (tf->sstatus & ~SSTATUS_SPP) | 0; // SPP = 0 (User)
        tf->sp = current->ustack;
        // 通知 trap_vec.S 切换页表
        user_satp = MAKE_SATP(current->pagetable);
        switch_to_user = 1;
        
        return 0;
    }
    return ret;
}

static void sys_exit(int status) {
    current->exit_code = status;
    exit();
}

static uint64_t sys_dup2(uint64_t oldfd, uint64_t newfd) {
    return fs_dup2(oldfd, newfd);
}

static uint64_t sys_fork(void) {
    // 记录下 fork 前的父进程 PID
    int parent_pid = current->pid;
    
    int ret = fork(); 
    
    // 因为 fork 完美复制了内核栈，子进程被调度唤醒后也会回到这里！
    // 此时对比一下当前执行流的 PID，如果变了，说明现在是子进程的躯壳在运行。
    if (current->pid != parent_pid) {
        return 0; // 子进程必须返回 0
    }
    return ret;   // 父进程返回子进程的 PID
}

static uint64_t sys_wait(int pid, int *status) {
    return wait(pid, status);
}

static uint64_t sys_get_tick(void) {
    return r_time();
}

void handle_syscall(struct trapframe *tf) {
    // 【核心修复】：在处理可能阻塞的系统调用期间，必须开放 S 模式的中断接收！
    // 否则阻塞时的 yield() 将导致 UART 中断永远无法打断 CPU，键盘必定卡死。
    w_sstatus(r_sstatus() | SSTATUS_SIE);

    uint64_t syscall_num = tf->a7;
    uint64_t ret_val = 0;

    switch (syscall_num) {
        case SYS_write:
            ret_val = sys_write(tf->a0, (const char *)tf->a1, tf->a2);
            break;
        case SYS_read:
            ret_val = sys_read(tf->a0, (char *)tf->a1, tf->a2);
            break;
        case SYS_open:
            ret_val = sys_open((const char *)tf->a0, tf->a1);
            break;
        case SYS_close:
            ret_val = sys_close(tf->a0);
            break;
        case SYS_opendir:
            ret_val = sys_opendir((const char *)tf->a0);
            break;
        case SYS_readdir:
            ret_val = sys_readdir(tf->a0, (char *)tf->a1);
            break;
        case SYS_exec:
            ret_val = sys_exec((const char *)tf->a0, (char **)tf->a1, tf);
            break;
        case SYS_exit:
            sys_exit((int)tf->a0);
            break;   // 不会返回
        case SYS_dup2:
            ret_val = sys_dup2(tf->a0, tf->a1);
            break;
        case SYS_fork:
            ret_val = sys_fork();
            break;
        case SYS_wait:
            ret_val = sys_wait((int)tf->a0, (int *)tf->a1);
            break;
        case SYS_get_tick:
            ret_val = sys_get_tick();
            break;
        default:
            printf("[Syscall] Unknown Syscall ID: %ld\n", syscall_num);
            ret_val = -1;
            break;
    }
    tf->a0 = ret_val;
}

