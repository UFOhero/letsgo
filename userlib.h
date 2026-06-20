#ifndef USERLIB_H
#define USERLIB_H
#include <stddef.h>
#include <stdint.h>

#define O_RDONLY    0x01
#define O_WRONLY    0x02
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_CREAT     0x04
#define O_TRUNC     0x08   // <--- 加在这里！分配下一个二进制位 0x08
#define TERMINAL_INODE -1

int write(int fd, const void *buf, int count);
int read(int fd, void *buf, int count);
int open(const char *path, int flags);
int close(int fd);
int exec(const char *path, char *const argv[]);
void exit(int status);
int fork(void);
int wait(int pid, int *status);
int opendir(const char *path);
int readdir(int fd, char *name);
int dup2(int oldfd, int newfd);
uint64_t get_tick(void);
size_t strlen(const char *s);

int ps(void);
int kill(int pid);

#endif
