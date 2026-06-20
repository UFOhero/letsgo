#include "userlib.h"

// 简单的字符串转数字函数
int atoi(const char *str) {
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        // 如果你的 userlib 支持 printf，就用 printf，否则这里可以直接写死退出
        // printf("Usage: kill <pid>\n");
        char usage[] = "Usage: kill <pid>\n";
        write(1, usage, sizeof(usage) - 1);
        return -1;
    }

    int target_pid = atoi(argv[1]);
    int ret = kill(target_pid);
    
    if (ret < 0) {
        char err[] = "kill: failed or permission denied.\n";
        write(1, err, sizeof(err) - 1);
    } else {
        char succ[] = "kill: success.\n";
        write(1, succ, sizeof(succ) - 1);
    }
    
    return 0;
}