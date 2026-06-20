#include "testlib.h"

static int read_file_text(const char *path, char *buf, int cap) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    int n = read(fd, buf, cap - 1);
    if (n >= 0) buf[n] = 0;
    close(fd);
    return n;
}

static void require_ok(int cond, const char *name) {
    if (cond) {
        t_pass(name);
    } else {
        t_fail(name);
        exit(1);
    }
}

static int dir_contains(const char *dir_path, const char *target) {
    int fd = opendir(dir_path);
    if (fd < 0) return 0;

    char name[32];
    int found = 0;
    while (readdir(fd, name) > 0) {
        if (t_streq(name, target)) {
            found = 1;
            break;
        }
    }
    close(fd);
    return found;
}

static void test_root_and_initial_dirs(void) {
    t_puts("\n  [用例1] 根目录与初始目录项遍历\n");

    require_ok(dir_contains("/", "bin"), "根目录包含 /bin");
    require_ok(dir_contains("/", "tmp"), "根目录包含 /tmp");
    require_ok(dir_contains("/", "hello.txt"), "根目录包含初始化文件 hello.txt");
}

static void test_create_write_read(void) {
    t_puts("\n  [用例2] 文件创建、连续写入与完整读取\n");

    int ret = mkdir("/tmp/fs_case");
    if (ret < 0) {
        int dir = opendir("/tmp/fs_case");
        if (dir < 0) {
            t_fail("创建测试目录 /tmp/fs_case");
            exit(1);
        }
        close(dir);
    }
    mkdir("/tmp/fs_case/sub");

    int fd = open("/tmp/./fs_case/sub/../alpha.txt", O_CREAT | O_RDWR | O_TRUNC);
    require_ok(fd >= 0, "使用 . 和 .. 路径创建文件");

    int n1 = write(fd, "alpha-", 6);
    int n2 = write(fd, "beta", 4);
    close(fd);
    require_ok(n1 == 6 && n2 == 4, "连续 write 更新文件偏移和大小");

    char buf[64];
    int n = read_file_text("/tmp/fs_case/alpha.txt", buf, sizeof(buf));
    require_ok(n == 10 && t_streq(buf, "alpha-beta"), "重新打开文件后读取完整内容");
}

static void test_lseek_and_eof(void) {
    t_puts("\n  [用例3] 文件偏移、lseek 与 EOF\n");

    int fd = open("/tmp/fs_case/alpha.txt", O_RDONLY);
    require_ok(fd >= 0, "打开已有文件用于偏移测试");

    char buf[16];
    int pos = lseek(fd, 6, SEEK_SET);
    int n = read(fd, buf, 4);
    if (n >= 0) buf[n] = 0;
    require_ok(pos == 6 && n == 4 && t_streq(buf, "beta"), "SEEK_SET 后从指定偏移读取");

    n = read(fd, buf, 4);
    require_ok(n == 0, "读到文件末尾返回 EOF");

    pos = lseek(fd, -5, SEEK_END);
    n = read(fd, buf, 5);
    if (n >= 0) buf[n] = 0;
    close(fd);
    require_ok(pos == 5 && n == 5 && t_streq(buf, "-beta"), "SEEK_END 支持相对文件尾定位");
}

static void test_truncate(void) {
    t_puts("\n  [用例4] O_TRUNC 截断已有文件\n");

    int fd = open("/tmp/fs_case/alpha.txt", O_RDWR | O_TRUNC);
    require_ok(fd >= 0, "以 O_TRUNC 打开已有文件");
    int n = write(fd, "new", 3);
    close(fd);
    require_ok(n == 3, "截断后重新写入短内容");

    char buf[32];
    n = read_file_text("/tmp/fs_case/alpha.txt", buf, sizeof(buf));
    require_ok(n == 3 && t_streq(buf, "new"), "截断后旧内容不会残留");
}

static void test_dup2(void) {
    t_puts("\n  [用例5] 文件描述符表与 dup2\n");

    int fd = open("/tmp/fs_case/dup.txt", O_CREAT | O_RDWR | O_TRUNC);
    require_ok(fd >= 0, "创建 dup2 测试文件");

    int newfd = dup2(fd, 10);
    require_ok(newfd == 10, "dup2 返回指定的新文件描述符");
    close(fd);

    int n = write(newfd, "dup-data", 8);
    close(newfd);
    require_ok(n == 8, "关闭旧 fd 后仍可通过 dup fd 写入");

    char buf[32];
    n = read_file_text("/tmp/fs_case/dup.txt", buf, sizeof(buf));
    require_ok(n == 8 && t_streq(buf, "dup-data"), "dup2 写入内容可被重新读取");
}

static void test_directory_traversal(void) {
    t_puts("\n  [用例6] 子目录遍历与目录项维护\n");

    require_ok(dir_contains("/tmp/fs_case", "."), "目录包含 . 项");
    require_ok(dir_contains("/tmp/fs_case", ".."), "目录包含 .. 项");
    require_ok(dir_contains("/tmp/fs_case", "sub"), "目录遍历可发现子目录");
    require_ok(dir_contains("/tmp/fs_case", "alpha.txt"), "目录遍历可发现普通文件");
    require_ok(dir_contains("/tmp/fs_case", "dup.txt"), "目录遍历可发现 dup2 创建的文件");
}

static void test_unlink(void) {
    t_puts("\n  [用例7] unlink 删除文件与引用计数保护\n");

    int fd = open("/tmp/fs_case/remove.txt", O_CREAT | O_RDWR | O_TRUNC);
    require_ok(fd >= 0, "创建待删除文件");
    write(fd, "remove", 6);

    int ret = unlink("/tmp/fs_case/remove.txt");
    require_ok(ret < 0, "已打开文件不能被 unlink 删除");
    close(fd);

    ret = unlink("/tmp/fs_case/remove.txt");
    require_ok(ret == 0, "关闭后 unlink 删除文件成功");

    fd = open("/tmp/fs_case/remove.txt", O_RDONLY);
    require_ok(fd < 0, "删除后的文件不能再次打开");
}

static void test_fd_limit(void) {
    t_puts("\n  [用例8] 文件描述符上限与 close 回收\n");

    int fds[20];
    int count = 0;
    for (int i = 0; i < 20; i++) {
        int fd = open("/tmp/fs_case/alpha.txt", O_RDONLY);
        if (fd < 0) break;
        fds[count++] = fd;
    }

    require_ok(count > 0 && count <= 13, "文件描述符表存在容量上限");

    int extra = open("/tmp/fs_case/alpha.txt", O_RDONLY);
    require_ok(extra < 0, "fd 表耗尽后 open 返回失败");

    for (int i = 0; i < count; i++) {
        close(fds[i]);
    }

    extra = open("/tmp/fs_case/alpha.txt", O_RDONLY);
    require_ok(extra >= 0, "close 后 fd 可被重新分配");
    close(extra);
}

__attribute__((section(".text.entry")))
void _start(int argc, char **argv) {
    (void)argc;
    (void)argv;

    t_puts("\n[文件系统模块测试] RAMFS/VFS 综合验证\n");
    t_puts("  覆盖指标: 路径解析、目录项、inode、文件描述符、读写偏移、dup2、truncate、unlink\n");

    test_root_and_initial_dirs();
    test_create_write_read();
    test_lseek_and_eof();
    test_truncate();
    test_dup2();
    test_directory_traversal();
    test_unlink();
    test_fd_limit();

    t_puts("\n  [总结] 文件系统相关检查完成\n");
    exit(0);
}
