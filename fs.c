#include "fs.h"
#include "string.h"

#define MAX_DIR_ENTRIES 16

inode_t inode_table[MAX_INODES];
char    data_buf[DATA_BUF_SIZE];
static int next_free_offset = 0;

file_desc_t current_fd_table[MAX_FD];
int         current_cwd = 0;

extern int printf(const char *fmt, ...);

static int alloc_inode(int type) {
    for (int i = 0; i < MAX_INODES; i++) {
        if (inode_table[i].used == 0) {
            inode_table[i].used = 1;
            inode_table[i].type = type;
            inode_table[i].size = 0;
            inode_table[i].data_offset = 0;
            inode_table[i].ref_count = 0;
            return i;
        }
    }
    return -1;
}

static int dir_add_entry(int dir_inode, const char *name, int target_inode) {
    if (inode_table[dir_inode].type != INODE_DIR) return -1;
    int entry_size = sizeof(dir_entry_t);
    if (inode_table[dir_inode].size + entry_size > MAX_DIR_ENTRIES * entry_size) return -1;
    if (inode_table[dir_inode].size == 0) {
        if (next_free_offset + MAX_DIR_ENTRIES * entry_size > DATA_BUF_SIZE) return -1;
        inode_table[dir_inode].data_offset = next_free_offset;
        next_free_offset += MAX_DIR_ENTRIES * entry_size;
    }
    dir_entry_t entry;
    strncpy(entry.name, name, MAX_FILENAME);
    entry.inode = target_inode;
    memcpy(data_buf + inode_table[dir_inode].data_offset + inode_table[dir_inode].size,
           &entry, entry_size);
    inode_table[dir_inode].size += entry_size;
    return 0;
}

static int dir_lookup(int dir_inode, const char *name) {
    if (inode_table[dir_inode].type != INODE_DIR) return -1;
    int num = inode_table[dir_inode].size / sizeof(dir_entry_t);
    dir_entry_t *entries = (dir_entry_t *)(data_buf + inode_table[dir_inode].data_offset);
    for (int i = 0; i < num; i++) {
        int ino = entries[i].inode;
        if (ino >= 0 && ino < MAX_INODES && inode_table[ino].used &&
            strncmp(entries[i].name, name, MAX_FILENAME) == 0)
            return entries[i].inode;
    }
    return -1;
}

int fs_resolve_path(const char *path, int *out_inode) {
    if (!path || !out_inode) {
        return -1;
    }
    int cur;
    const char *p = path;
    if (p[0] == '/') { cur = 0; p++; }
    else cur = current_cwd;
    char tmp[256];
    strncpy(tmp, p, 255);
    tmp[255] = 0;
    char *token = strtok(tmp, "/");
    while (token) {
        if (cur < 0 || inode_table[cur].type != INODE_DIR) {
            return -1;
        }
        if (strcmp(token, ".") == 0) { }
        else if (strcmp(token, "..") == 0) {
            int parent = dir_lookup(cur, "..");
            if (parent < 0) {
                return -1;
            }
            cur = parent;
        } else {
            int next = dir_lookup(cur, token);
            if (next < 0) {
                return -1;
            }
            cur = next;
        }
        token = strtok(NULL, "/");
    }
    *out_inode = cur;
    return 0;
}

void fs_init(void) {
    memset(inode_table, 0, sizeof(inode_table));
    memset(data_buf, 0, sizeof(data_buf));
    next_free_offset = 0;
    int root = alloc_inode(INODE_DIR);
    dir_add_entry(root, ".", root);
    dir_add_entry(root, "..", root);
    for (int i = 0; i < MAX_FD; i++) current_fd_table[i].valid = 0;
    current_cwd = 0;
    current_fd_table[0].valid = 1;
    current_fd_table[0].inode = TERMINAL_INODE;
    current_fd_table[0].flags = 0;
    current_fd_table[0].offset = 0;
    current_fd_table[1].valid = 1;
    current_fd_table[1].inode = TERMINAL_INODE;
    current_fd_table[1].flags = 0;
    current_fd_table[1].offset = 0;
    current_fd_table[2].valid = 1;
    current_fd_table[2].inode = TERMINAL_INODE;
    current_fd_table[2].flags = 0;
    current_fd_table[2].offset = 0;
}

int fs_open(const char *path, int flags) {
    int inode_num;
    if (fs_resolve_path(path, &inode_num) != 0) {
        // 文件不存在，尝试创建
        if (flags & O_CREAT) {
            char parent_path[256], name[MAX_FILENAME];
            const char *last = strrchr(path, '/');
            if (last) {
                int len = last - path;
                if (len == 0) strcpy(parent_path, "/");
                else { strncpy(parent_path, path, len); parent_path[len]=0; }
                strncpy(name, last+1, MAX_FILENAME);
            } else {
                strcpy(parent_path, ".");
                strncpy(name, path, MAX_FILENAME);
            }
            int parent;
            if (fs_resolve_path(parent_path, &parent) != 0) return -1;
            int new_inode = alloc_inode(INODE_FILE);
            if (new_inode < 0) return -1;
            dir_add_entry(parent, name, new_inode);
            inode_num = new_inode;
        } else return -1;
    } else {
        if (!inode_table[inode_num].used || inode_table[inode_num].type != INODE_FILE) {
            return -1;
        }
        // 【核心修复】：文件已存在，如果带有 O_TRUNC 标志且为写入模式，将文件大小截断为 0
        if ((flags & O_TRUNC) && (flags & (O_WRONLY | O_RDWR))) {
            inode_table[inode_num].size = 0;
        }
    }

    for (int i = 3; i < MAX_FD; i++) {
        if (!current_fd_table[i].valid) {
            current_fd_table[i].inode = inode_num;
            current_fd_table[i].offset = 0;
            current_fd_table[i].flags = flags;
            current_fd_table[i].valid = 1;
            inode_table[inode_num].ref_count++;
            return i;
        }
    }
    return -1;
}

int fs_close(int fd) {
    if (fd < 0 || fd >= MAX_FD || !current_fd_table[fd].valid) return -1;
    int ino = current_fd_table[fd].inode;
    current_fd_table[fd].valid = 0;
    if (ino != TERMINAL_INODE && ino >= 0 && inode_table[ino].ref_count > 0)
        inode_table[ino].ref_count--;
    return 0;
}

int fs_read(int fd, void *buf, int count) {
    if (fd < 0 || fd >= MAX_FD || !current_fd_table[fd].valid) return -1;
    file_desc_t *f = &current_fd_table[fd];
    
    // 【核心修复 1】：将终端读操作接入 VFS，并支持 Ctrl+D 作为 EOF
    if (f->inode == TERMINAL_INODE) {
        extern char uart_getc_blocking(void);
        if (count > 0) {
            char c = uart_getc_blocking();
            // 0x04 是 Ctrl+D 的 ASCII 码，代表 End of Transmission (EOF)
            if (c == 0x04) {
                return 0; // 返回 0，代表文件读完了，cat 接收到 0 就会退出循环
            }
            ((char*)buf)[0] = c;
            return 1;
        }
        return 0; 
    }
    
    // 以下是原来的读取文件的逻辑，保持不变
    inode_t *ino = &inode_table[f->inode];
    if (ino->type != INODE_FILE) return -1;
    if (f->offset >= ino->size) return 0;
    int readable = ino->size - f->offset;
    if (count > readable) count = readable;
    memcpy(buf, data_buf + ino->data_offset + f->offset, count);
    f->offset += count;
    return count;
}

int fs_write(int fd, const void *buf, int count) {
    if (fd < 0 || fd >= MAX_FD || !current_fd_table[fd].valid) return -1;
    file_desc_t *f = &current_fd_table[fd];
    if (f->inode == TERMINAL_INODE) {
        extern void uart_putc(char c);
        for (int i = 0; i < count; i++) uart_putc(((char*)buf)[i]);
        return count;
    }
    inode_t *ino = &inode_table[f->inode];
    if (ino->type != INODE_FILE) return -1;
    if (f->offset + count > MAX_FILE_SIZE) return -1;
    if (ino->size == 0) {
        if (next_free_offset + MAX_FILE_SIZE > DATA_BUF_SIZE) return -1;
        ino->data_offset = next_free_offset;
        next_free_offset += MAX_FILE_SIZE;
    }
    memcpy(data_buf + ino->data_offset + f->offset, buf, count);
    if (f->offset + count > ino->size) ino->size = f->offset + count;
    f->offset += count;
    return count;
}

int fs_lseek(int fd, int offset, int whence) {
    if (fd < 0 || fd >= MAX_FD || !current_fd_table[fd].valid) return -1;
    file_desc_t *f = &current_fd_table[fd];
    if (f->inode < 0 || f->inode >= MAX_INODES) return -1;
    inode_t *ino = &inode_table[f->inode];
    if (ino->type != INODE_FILE) return -1;
    switch (whence) {
        case 0: f->offset = offset; break;
        case 1: f->offset += offset; break;
        case 2: f->offset = ino->size + offset; break;
        default: return -1;
    }
    if (f->offset < 0) f->offset = 0;
    if (f->offset > ino->size) f->offset = ino->size;
    return f->offset;
}

int fs_mkdir(const char *path) {
    char parent_path[256], name[MAX_FILENAME];
    const char *last = strrchr(path, '/');
    if (last) {
        int len = last - path;
        if (len == 0) strcpy(parent_path, "/");
        else { strncpy(parent_path, path, len); parent_path[len]=0; }
        strncpy(name, last+1, MAX_FILENAME);
    } else {
        strcpy(parent_path, ".");
        strncpy(name, path, MAX_FILENAME);
    }
    int parent;
    if (fs_resolve_path(parent_path, &parent) != 0) return -1;
    if (dir_lookup(parent, name) >= 0) return -1;
    int new_dir = alloc_inode(INODE_DIR);
    if (new_dir < 0) return -1;
    dir_add_entry(new_dir, ".", new_dir);
    dir_add_entry(new_dir, "..", parent);
    dir_add_entry(parent, name, new_dir);
    return 0;
}

int fs_unlink(const char *path) {
    int ino;
    if (fs_resolve_path(path, &ino) != 0) return -1;
    if (inode_table[ino].type != INODE_FILE) return -1;
    if (inode_table[ino].ref_count > 0) return -1;
    inode_table[ino].used = 0;
    inode_table[ino].type = INODE_FREE;
    return 0;
}

int fs_opendir(const char *path) {
    int ino;
    int res = fs_resolve_path(path, &ino);
    if (res != 0) {
        return -1;
    }
    if (inode_table[ino].type != INODE_DIR) {
        return -1;
    }
    for (int i = 3; i < MAX_FD; i++) {
        if (!current_fd_table[i].valid) {
            current_fd_table[i].inode = ino;
            current_fd_table[i].offset = 0;
            current_fd_table[i].flags = 0;
            current_fd_table[i].valid = 1;
            inode_table[ino].ref_count++;
            return i;
        }
    }
    return -1;
}

int fs_readdir(int fd, char *name) {
    if (fd < 0 || fd >= MAX_FD || !current_fd_table[fd].valid) return -1;
    file_desc_t *f = &current_fd_table[fd];
    inode_t *ino = &inode_table[f->inode];
    if (ino->type != INODE_DIR) return -1;
    int idx = f->offset;
    int num_entries = ino->size / sizeof(dir_entry_t);
    while (idx < num_entries) {
        dir_entry_t *e = (dir_entry_t*)(data_buf + ino->data_offset) + idx;
        if (e->inode >= 0 && e->inode < MAX_INODES && inode_table[e->inode].used) {
            strncpy(name, e->name, MAX_FILENAME);
            f->offset = idx + 1;
            return 1;
        }
        idx++;
    }
    return 0;
}

int fs_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= MAX_FD || !current_fd_table[oldfd].valid) return -1;
    if (newfd < 0 || newfd >= MAX_FD) return -1;
    if (oldfd == newfd) return newfd;
    if (current_fd_table[newfd].valid) fs_close(newfd);
    current_fd_table[newfd] = current_fd_table[oldfd];
    current_fd_table[newfd].valid = 1;
    if (current_fd_table[oldfd].inode != TERMINAL_INODE)
        inode_table[current_fd_table[oldfd].inode].ref_count++;
    return newfd;
}

// 创建测试文件（内嵌 ELF 数据将在 userlib.c 中提供）
extern unsigned char hello_elf[], ls_elf[], cat_elf[], echo_elf[], null_elf[];
extern unsigned int hello_elf_len, ls_elf_len, cat_elf_len, echo_elf_len, null_elf_len;
extern unsigned char test_boot_elf[], test_intr_elf[], test_mem_elf[];
extern unsigned char test_proc_elf[], test_fs_elf[], test_exec_elf[];
extern unsigned char test_sched_elf[];
extern unsigned int test_boot_elf_len, test_intr_elf_len, test_mem_elf_len;
extern unsigned int test_proc_elf_len, test_fs_elf_len, test_exec_elf_len;
extern unsigned int test_sched_elf_len;

static void install_user_program(const char *path, unsigned char *elf, unsigned int len) {
    int fd = fs_open(path, O_RDWR | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        fs_write(fd, elf, len);
        fs_close(fd);
    } else {
        printf("[create_user_files] failed to install %s\n", path);
    }
}

void create_user_files(void) {
    int fd;
    fd = fs_open("/hello.txt", O_RDWR | O_CREAT);
    if (fd >= 0) { fs_write(fd, "Hello, world!\n", 14); fs_close(fd); }
    fs_mkdir("/bin");
    fs_mkdir("/tmp");

    printf("[Init] Installing user programs into /bin...\n");
    install_user_program("/bin/hello", hello_elf, hello_elf_len);
    install_user_program("/bin/ls", ls_elf, ls_elf_len);
    install_user_program("/bin/cat", cat_elf, cat_elf_len);
    install_user_program("/bin/echo", echo_elf, echo_elf_len);
    install_user_program("/bin/null", null_elf, null_elf_len);

    printf("[Init] Installing test programs into /bin...\n");
    install_user_program("/bin/test_boot", test_boot_elf, test_boot_elf_len);
    install_user_program("/bin/test_intr", test_intr_elf, test_intr_elf_len);
    install_user_program("/bin/test_mem", test_mem_elf, test_mem_elf_len);
    install_user_program("/bin/test_proc", test_proc_elf, test_proc_elf_len);
    install_user_program("/bin/test_fs", test_fs_elf, test_fs_elf_len);
    install_user_program("/bin/test_exec", test_exec_elf, test_exec_elf_len);
    install_user_program("/bin/test_sched", test_sched_elf, test_sched_elf_len);
}
