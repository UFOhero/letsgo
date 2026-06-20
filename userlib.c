#include "userlib.h"
#include "syscall.h"

int write(int fd, const void *buf, int count) {
    register uint64_t a0 asm("a0") = fd;
    register uint64_t a1 asm("a1") = (uint64_t)buf;
    register uint64_t a2 asm("a2") = count;
    register uint64_t a7 asm("a7") = SYS_write;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

int read(int fd, void *buf, int count) {
    register uint64_t a0 asm("a0") = fd;
    register uint64_t a1 asm("a1") = (uint64_t)buf;
    register uint64_t a2 asm("a2") = count;
    register uint64_t a7 asm("a7") = SYS_read;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

int open(const char *path, int flags) {
    register uint64_t a0 asm("a0") = (uint64_t)path;
    register uint64_t a1 asm("a1") = flags;
    register uint64_t a7 asm("a7") = SYS_open;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
    return a0;
}

int close(int fd) {
    register uint64_t a0 asm("a0") = fd;
    register uint64_t a7 asm("a7") = SYS_close;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

int exec(const char *path, char *const argv[]) {
    register uint64_t a0 asm("a0") = (uint64_t)path;
    register uint64_t a1 asm("a1") = (uint64_t)argv;
    register uint64_t a7 asm("a7") = SYS_exec;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
    return a0;
}

void exit(int status) {
    register uint64_t a0 asm("a0") = status;
    register uint64_t a7 asm("a7") = SYS_exit;
    asm volatile("ecall" : : "r"(a0), "r"(a7) : "memory");
    while(1);
}

int opendir(const char *path) {
    register uint64_t a0 asm("a0") = (uint64_t)path;
    register uint64_t a7 asm("a7") = SYS_opendir;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

int readdir(int fd, char *name) {
    register uint64_t a0 asm("a0") = fd;
    register uint64_t a1 asm("a1") = (uint64_t)name;
    register uint64_t a7 asm("a7") = SYS_readdir;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
    return a0;
}

int dup2(int oldfd, int newfd) {
    register uint64_t a0 asm("a0") = oldfd;
    register uint64_t a1 asm("a1") = newfd;
    register uint64_t a7 asm("a7") = SYS_dup2;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
    return a0;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int fork(void) {
    register uint64_t a0 asm("a0");
    register uint64_t a7 asm("a7") = SYS_fork;
    asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
    return a0;
}

int wait(int pid, int *status) {
    register uint64_t a0 asm("a0") = pid;
    register uint64_t a1 asm("a1") = (uint64_t)status;
    register uint64_t a7 asm("a7") = SYS_wait;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
    return a0;
}