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

volatile uint64_t trap_timer_count __attribute__((weak));
volatile uint64_t trap_uart_count __attribute__((weak));
volatile uint64_t trap_syscall_count __attribute__((weak));
volatile uint64_t trap_page_fault_count __attribute__((weak));

extern void do_ps(void);
extern int do_kill(int pid);

static uint64_t sys_write(uint64_t fd, const char *buf, uint64_t count) {

    int ret = fs_write(fd, buf, count);
    
    // 如果 VFS 成功处理了（返回值 >= 0），任务完成！
    if (ret >= 0) {
        return ret; 
    }

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

    int ret = fs_read(fd, buf, count);
    
    if (ret >= 0) {
        return ret;
    }

    if (fd == 0) {
        extern char uart_getc_blocking(void);
        if (count > 0) {
            char c = uart_getc_blocking();
            if (c == 0x04) return 0; // 兜底处同样支持 Ctrl+D
            buf[0] = c;
            return 1;
        }
        return 0;
    }
    
    return -1;
}

static uint64_t sys_open(const char *path, uint64_t flags) {
    return fs_open(path, flags);
}

static uint64_t sys_close(uint64_t fd) {
    return fs_close(fd);
}

static uint64_t sys_opendir(const char *path) {
    return fs_opendir(path);
}

static uint64_t sys_readdir(uint64_t fd, char *name) {
    return fs_readdir(fd, name);
}

static uint64_t sys_exec(const char *path, char *const argv[], struct trapframe *tf) {
    uint64_t u_argc = 0, u_argv = 0;
    int ret = exec(path, argv, &u_argc, &u_argv);
    if (ret == 0) {
        // 设置 trapframe 以返回用户态
        tf->sepc = current->entry;
        tf->sstatus = (tf->sstatus & ~SSTATUS_SPP) | 0; // SPP = 0 (User)
        tf->sp = current->ustack;
        tf->a1 = u_argv;
        // 通知 trap_vec.S 切换页表
        user_satp = MAKE_SATP(current->pagetable);
        switch_to_user = 1;
        
        return u_argc;
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

static uint64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence) {
    return fs_lseek(fd, (int)offset, (int)whence);
}

static uint64_t sys_mkdir(const char *path) {
    return fs_mkdir(path);
}

static uint64_t sys_unlink(const char *path) {
    return fs_unlink(path);
}

static uint64_t sys_fork(struct trapframe *tf) {
    return fork_from_trap(tf);
}


static uint64_t sys_wait(int pid, int *status) {
    return wait(pid, status);
}

static uint64_t sys_get_tick(void) {
    return r_time();
}

static uint64_t sys_get_trap_count(uint64_t type) {
    extern volatile uint64_t trap_timer_count;
    extern volatile uint64_t trap_uart_count;
    extern volatile uint64_t trap_syscall_count;
    extern volatile uint64_t trap_page_fault_count;

    switch (type) {
        case 0: return trap_timer_count;
        case 1: return trap_uart_count;
        case 2: return trap_syscall_count;
        case 3: return trap_page_fault_count;
        default: return (uint64_t)-1;
    }
}

static uint64_t sys_get_sched_info(int *algorithm, int *pid, int *priority,
                                   uint64_t *slice, int *need_resched_out) {
    extern int sched_algorithm;
    extern volatile int need_resched;

    if (!current) return -1;
    if (algorithm) *algorithm = sched_algorithm;
    if (pid) *pid = (int)current->pid;
    if (priority) *priority = current->priority;
    if (slice) *slice = current->slice;
    if (need_resched_out) *need_resched_out = need_resched;
    return 0;
}

static uint64_t sys_yield(void) {
    yield();
    return 0;
}

void handle_syscall(struct trapframe *tf) {

    uint64_t current_sstatus = r_sstatus();
    w_sstatus(current_sstatus | (1ULL << 18));

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
            ret_val = sys_fork(tf);
            break;
        case SYS_wait:
            ret_val = sys_wait((int)tf->a0, (int *)tf->a1);
            break;
        case SYS_get_tick:
            ret_val = sys_get_tick();
            break;
        case SYS_yield:
            ret_val = sys_yield();
            break;
        case SYS_lseek:
            ret_val = sys_lseek(tf->a0, tf->a1, tf->a2);
            break;
        case SYS_mkdir:
            ret_val = sys_mkdir((const char *)tf->a0);
            break;
        case SYS_unlink:
            ret_val = sys_unlink((const char *)tf->a0);
            break;
        case SYS_get_trap_count:
            ret_val = sys_get_trap_count(tf->a0);
            break;
        case SYS_get_sched_info:
            ret_val = sys_get_sched_info((int *)tf->a0, (int *)tf->a1, (int *)tf->a2,
                                         (uint64_t *)tf->a3, (int *)tf->a4);
            break;
        case SYS_ps:
            do_ps();
            ret_val = 0;
            break;
        case SYS_kill:
            ret_val = do_kill((int)tf->a0);
            break;
        default:
            printf("[Syscall] Unknown Syscall ID: %ld\n", syscall_num);
            ret_val = -1;
            break;
    }
    tf->a0 = ret_val;
}

