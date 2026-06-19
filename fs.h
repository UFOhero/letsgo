#ifndef FS_H
#define FS_H
#include <stdint.h>

#define MAX_INODES      128
#define MAX_FILENAME    28
#define MAX_FILE_SIZE   (64*1024)
#define DATA_BUF_SIZE   (128 * 64 * 1024)

#define INODE_FREE  0
#define INODE_FILE  1
#define INODE_DIR   2

#define O_RDONLY    0x01
#define O_WRONLY    0x02
#define O_RDWR      (O_RDONLY | O_WRONLY)
#define O_CREAT     0x04
#define O_TRUNC     0x08   // <--- 加在这里！分配下一个二进制位 0x08
#define TERMINAL_INODE -1

typedef struct {
    char name[MAX_FILENAME];
    int  inode;
} dir_entry_t;

typedef struct {
    int   type;
    int   size;
    int   data_offset;
    int   used;
    int   ref_count;
} inode_t;

typedef struct {
    int   inode;
    int   offset;
    int   flags;
    int   valid;
} file_desc_t;

#define MAX_FD         16

extern file_desc_t current_fd_table[MAX_FD];
extern int         current_cwd;

void fs_init(void);
int  fs_open(const char *path, int flags);
int  fs_close(int fd);
int  fs_read(int fd, void *buf, int count);
int  fs_write(int fd, const void *buf, int count);
int  fs_lseek(int fd, int offset, int whence);
int  fs_mkdir(const char *path);
int  fs_unlink(const char *path);
int  fs_opendir(const char *path);
int  fs_readdir(int fd, char *name);
int  fs_dup2(int oldfd, int newfd);
int  fs_resolve_path(const char *path, int *out_inode);

#endif