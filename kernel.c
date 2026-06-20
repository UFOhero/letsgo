#include <stdint.h>
#include "csr.h"
#include "proc.h"
#include "fs.h"
#include "syscall.h"
#include "string.h"

extern void uart_init(void);
extern int printf(const char *fmt, ...);
extern void trap_init(void);
extern void plic_init(void);
extern void timer_init(void);
extern void pmm_init(uint64_t mem_start, uint64_t mem_end);
extern void vmm_init(void);
extern void heap_init(void);
extern void *kmalloc(uint64_t size);
extern void kfree(void *ptr);
extern char _end;
extern uint64_t *kernel_pagetable;
extern void fs_init(void);
extern void create_user_files(void);

extern void uart_putc(char c);
extern char uart_getc_blocking(void);

static int getchar(void) {
    return uart_getc_blocking(); // 使用带睡眠机制的阻塞读取
}

static int readline(char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        int c = getchar();
        if (c == '\r' || c == '\n') {
            buf[i] = '\0'; uart_putc('\n'); return i;
        } else if (c == '\b' || c == 127) {
            if (i > 0) { i--; uart_putc('\b'); uart_putc(' '); uart_putc('\b'); }
        } else {
            buf[i++] = c; uart_putc(c); // 自带回显
        }
    }
    buf[i] = '\0'; return i;
}

// 【彻底移除 ksyscall 宏，内核自己不准给自己发 ecall！】

#include "string.h"
#include "fs.h"
#include "proc.h"

// ==== 核心修复：全局变量池，跨越父子内核栈的桥梁 ====
static char exec_path[64];
static char exec_argv_buf[16][64];
static char *exec_argv[16];

static void execute_command(char *cmd) {
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    int saved_stdin  = fs_dup2(0, 12);
    int saved_stdout = fs_dup2(1, 13);
    if (saved_stdin < 0) saved_stdin = 0;
    if (saved_stdout < 0) saved_stdout = 1;

    char *argv[16]; int argc = 0;
    char *redirect_in = NULL, *redirect_out = NULL;

    char *token = strtok(cmd, " ");
    while (token && argc < 15) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " "); if (token) redirect_in = token;
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " "); if (token) redirect_out = token;
        } else { argv[argc++] = token; }
        token = strtok(NULL, " ");
    }
    argv[argc] = NULL;
    if (argc == 0) goto restore;

    if (redirect_in) {
        int fd = fs_open(redirect_in, O_RDONLY);
        if (fd >= 0) { fs_dup2(fd, 0); fs_close(fd); }
        else { printf("Cannot open input file: %s\n", redirect_in); goto restore; }
    }
    if (redirect_out) {
        int fd = fs_open(redirect_out, O_RDWR | O_CREAT | O_TRUNC);
        if (fd >= 0) { fs_dup2(fd, 1); fs_close(fd); }
        else { printf("Cannot open output file: %s\n", redirect_out); goto restore; }
    }

    // ==========================================
    // 【核心修复】：类 Unix 系统的 $PATH 寻址逻辑
    // 如果输入的命令不是路径，自动去 /bin/ 目录下寻找
    // ==========================================
    char path[64];
    if (argv[0][0] != '/' && argv[0][0] != '.') {
        strcpy(path, "/bin/");
        strcat(path, argv[0]);
    } else {
        strcpy(path, argv[0]);
    }

    // 将路径和参数安全地拷贝到全局区，防止 fork 后子进程栈丢失
    strcpy(exec_path, path);
    for (int i = 0; i < argc; i++) {
        strcpy(exec_argv_buf[i], argv[i]);
        exec_argv[i] = exec_argv_buf[i];
    }
    exec_argv[argc] = NULL;

    extern int fork(void);
    extern int wait(int pid, int *status);
    extern void exit(void);
    
    int pid = fork();
    if (pid < 0) {
        printf("fork failed\n");
    } else if (pid == 0) {
        // ---- 子进程世界 ----
        uint64_t u_argc, u_argv;
        int ret = exec(exec_path, exec_argv, &u_argc, &u_argv);
        
        if (ret == 0) {
            extern volatile int switch_to_user;
            extern uint64_t user_satp;
            
            uint64_t satp = ((8ULL << 60) | ((uint64_t)current->pagetable >> 12));
            uint64_t sstatus = r_sstatus();
            sstatus &= ~(1ULL << 8); // 设置进入 User 模式
            sstatus |= (1ULL << 5);  // 开启中断 SPIE
            
            // 【核心修复】：必须通知 trap_vec 系统我们要去用户态了！
            switch_to_user = 1;
            user_satp = satp;
            uint64_t kstack_top = current->kstack;
            
            __asm__ volatile(
                "csrw sscratch, %6\n"   // <--- 救命的寄存器！为下一次中断提供内核栈入口！
                "csrw satp, %0\n"
                "sfence.vma zero, zero\n"
                "csrw sstatus, %1\n"
                "csrw sepc, %2\n"
                "mv sp, %3\n"
                "mv a0, %4\n"
                "mv a1, %5\n"
                "sret\n"
                : : "r"(satp), "r"(sstatus), "r"(current->entry), "r"(current->ustack),
                    "r"(u_argc), "r"(u_argv), "r"(kstack_top)
                : "a0", "a1", "memory" 
            );
            while(1);
        }
        printf("exec failed: %s\n", exec_path);
        current->exit_code = 1;
        extern void exit(void);
        exit();
    } else {
        // ---- 父进程世界 ----
        int status;
        wait(pid, &status);
    }

restore:
    fs_dup2(saved_stdin, 0);   fs_close(saved_stdin);
    fs_dup2(saved_stdout, 1);  fs_close(saved_stdout);
}

void shell_loop() {
    printf("Shell started.\n");
    char cmdline[128];
    while (1) {
        printf("shell> ");
        int len = readline(cmdline, sizeof(cmdline));
        if (len == 0) continue;

        char *pipe_pos = NULL;
        for (int i = 0; cmdline[i]; i++) {
            if (cmdline[i] == '|') {
                pipe_pos = &cmdline[i]; *pipe_pos = '\0'; break;
            }
        }
        if (pipe_pos) {
            char lbuf[128], rbuf[128];
            strcpy(lbuf, cmdline); strcat(lbuf, " > /tmp/pipe");
            strcpy(rbuf, pipe_pos+1); strcat(rbuf, " < /tmp/pipe");
            execute_command(lbuf); execute_command(rbuf);
        } else {
            execute_command(cmdline);
        }
    }
}

void kernel_main(uint64_t hartid, uint64_t dtb_pa) {
    uart_init();
    printf("Booting RISC-V OS on Hart %ld...\n", hartid);
    uint64_t kernel_end = (uint64_t)&_end;
    uint64_t dtb_start = dtb_pa;
    uint64_t free_mem_start = kernel_end;
    uint64_t free_mem_end = dtb_start & ~4095ULL;
    trap_init();
    pmm_init(free_mem_start, free_mem_end);
    vmm_init();
    plic_init();
    heap_init();
    fs_init();
    create_user_files();
    timer_init();

    // 【核心修复 1】：必须在调度器启动前，合上总电闸！
    // 因为下方的 proc_init 会带着 CPU 飞向第一个进程，再也不会返回这里了
    w_sie(r_sie() | SIE_STIE | SIE_SEIE); 
    w_sstatus(r_sstatus() | SSTATUS_SIE | SSTATUS_SUM); 

    proc_init(); // 一去不复返的时光机
    
    // （删掉这里原本无用的 w_sie, w_sstatus 和 shell_loop）
}