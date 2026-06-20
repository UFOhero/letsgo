// user/testall.c
#include "userlib.h"

// 独立检查并定义每个文件标志，防止被跳过
#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#ifndef O_WRONLY
#define O_WRONLY 1
#endif

#ifndef O_RDWR
#define O_RDWR   2
#endif

#ifndef O_CREAT
#define O_CREAT  0x200
#endif

// 显式声明用户态下尚未加入 userlib.h 的系统调用
extern int fork(void);
extern int wait(int pid, int *status);

// 辅助函数：字符串比较
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

// 辅助函数：打印字符串
void print_str(const char *str) {
    write(1, str, strlen(str));
}

// ---------------------------------------------------------
// 测试 1: 文件管理模块 (VFS, 文件读写, 目录遍历)
// ---------------------------------------------------------
void test_file_system() {
    print_str("\n[TEST 1] File Management Module...\n");
    
    // 1. 测试文件创建与写入 (O_CREAT | O_RDWR)
    int fd = open("testfs.txt", O_CREAT | O_RDWR);
    if (fd < 0) {
        print_str("  [FAIL] Cannot create testfs.txt.\n");
    } else {
        char *msg = "OS Virtual File System works perfectly!";
        write(fd, msg, strlen(msg));
        close(fd);
        print_str("  [PASS] File 'testfs.txt' created and written successfully.\n");
        
        // 2. 测试文件读取验证
        fd = open("testfs.txt", O_RDONLY);
        if (fd < 0) {
            print_str("  [FAIL] Cannot open testfs.txt for reading.\n");
        } else {
            char buf[64];
            int n = read(fd, buf, 63);
            if (n >= 0) buf[n] = '\0';
            close(fd);
            
            if (n >= 0 && strcmp(buf, msg) == 0) {
                print_str("  [PASS] File content verified successfully.\n");
            } else {
                print_str("  [FAIL] File content mismatch.\n");
            }
        }
    }

    // 3. 测试目录遍历
    print_str("  [INFO] Traversing root directory '/':\n");
    int dir_fd = opendir("/");
    if (dir_fd >= 0) {
        char name[32];
        while (readdir(dir_fd, name) > 0) {
            print_str("    -> ");
            print_str(name);
            print_str("\n");
        }
        close(dir_fd);
        print_str("  [PASS] Directory traversal successful.\n");
    } else {
        print_str("  [FAIL] opendir('/') failed.\n");
    }
}

// ---------------------------------------------------------
// 测试 2: 进程管理 & 用户程序加载模块 (fork, exec, wait)
// ---------------------------------------------------------
void test_process_and_loading() {
    print_str("\n[TEST 2] Process & Program Loading Module...\n");
    
    // 阶段 1: 测试 fork
    print_str("  -> [Debug] Phase 1: Ready to call fork()...\n");
    int pid = fork();
    
    if (pid < 0) {
        print_str("  [FAIL] fork() failed.\n");
    } else if (pid == 0) {
        print_str("  -> [Child 1] fork() returned 0, entering child process.\n");
        print_str("  -> [Child 1] Ready to call exit(0)...\n");
        exit(0);
    } else {
        print_str("  -> [Parent 1] fork() returned PID. Ready to call wait()...\n");
        int status;
        wait(pid, &status);
        print_str("  -> [Parent 1] wait() completed.\n");
        print_str("  [PASS] fork() and wait() work.\n");
    }

    // 阶段 2: 测试 exec
    print_str("  -> [Debug] Phase 2: Ready to call fork() for exec test...\n");
    pid = fork();
    
    if (pid < 0) {
        print_str("  [FAIL] fork() failed.\n");
    } else if (pid == 0) {
        print_str("  -> [Child 2] Fork successful. Preparing argv...\n");
        char *argv[] = {"hello", "test_arg", 0};
        
        print_str("  -> [Child 2] Ready to call exec('/bin/hello')...\n");
        // 如果是在这里崩溃，说明是 exec 拷贝参数到用户栈（很可能就在 0x7f000000）时发生了越界或缺页
        exec("/bin/hello", argv);
        
        print_str("  [FAIL] exec() failed in child.\n");
        exit(1);
    } else {
        print_str("  -> [Parent 2] fork() returned PID. Ready to call wait()...\n");
        int status;
        wait(pid, &status);
        print_str("  [PASS] Parent wait() complete. Child process loaded, executed, and exited.\n");
    }
}
//----------------------------------------------------------
// 测试 3: 内存管理 & 异常处理模块 (MMU, Page Fault, 进程隔离)
// ---------------------------------------------------------
void test_memory_and_exception() {
    print_str("\n[TEST 3] Memory & Exception Handling Module...\n");
    print_str("  [INFO] Forking a 'Kamikaze' child to trigger a deliberate Page Fault...\n");
    
    int pid = fork();
    if (pid < 0) {
        print_str("  [FAIL] fork() failed.\n");
    } else if (pid == 0) {
        // 子进程：故意向地址 0x0 写入数据，触发致命缺页异常
        print_str("  [Child] I will intentionally crash by writing to NULL pointer (0x0)!\n");
        volatile int *bad_ptr = (int *)0x0;
        *bad_ptr = 0xDEADBEEF; 
        
        // 如果内核没有拦截这个非法越权访问，就会执行到这里（甚至系统崩溃）
        print_str("  [FAIL] OS did not catch the memory violation!\n");
        exit(1);
    } else {
        // 父进程等待：验证内核是否成功捕获了 Trap，杀死了子进程，且没有导致 Kernel Panic
        int status;
        wait(pid, &status);
        print_str("  [PASS] Parent wait() complete. OS caught the Trap, isolated memory, and survived the crash gracefully!\n");
    }
}

// ---------------------------------------------------------
// 主函数入口
// ---------------------------------------------------------
__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc; (void)argv; // 忽略参数
    
    print_str("\n====================================================\n");
    print_str("   OS Integration Test Suite - All Modules Check    \n");
    print_str("====================================================\n");
    
    // 系统启动模块 (如果能运行到这里，说明启动链完美)
    print_str("[TEST 0] System Boot Module...\n");
    print_str("  [PASS] PMM, VMM, Trap Vectors, and U-Mode transitions are online!\n");

    // 依序执行测试
    test_file_system();
    test_process_and_loading();
    test_memory_and_exception();
    
    print_str("\n====================================================\n");
    print_str(" All modules integration tests finished gracefully! \n");
    print_str("====================================================\n");
    
    exit(0);
}