#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char *dir, char *file) {
    char buf[512] = {0}, *p;
    int fd;
    struct dirent de;
    struct stat st;
    // 打开目录
    if ((fd = open(dir, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", dir);
        return;
    }
    // 判断路径长度
    if (strlen(dir) + 1 + DIRSIZ + 1 > sizeof(buf)) {
        fprintf(2, "find: path too long\n");
        close(fd);
        return;
    }
    /*
     * 假设dir指向 abc'\0'      其中'\0'为结束符号
     * strcpy(buf, dir);  使得     buf指向 abc'\0'
     * p = buf + strlen(buf);   使得p指向abc'\0'的结束符'\0'
     * *p++ = '/';    *p++先取出p指向的变量，然后再让地址p自增,使得'\0'变为/,且p指向/的下一字节
     * */
    strcpy(buf, dir);   //strcpy会拷贝字符串结束符号'\0'
    p = buf + strlen(buf);  //strlen不包含结束符'\0'
    *p++ = '/';     //*p++先取出p指向的变量，然后再让地址p自增
    // 遍历目录下文档
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        // 跳过当前目录和上级目录
        if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) {
            continue;
        }
        // 得到完整路径
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        // 获取当前文档的状态
        if (stat(buf, &st) < 0) {
            fprintf(2, "find: cannot stat %s\n", buf);
            continue;
        }
        // 是目录则递归遍历
        if (st.type == T_DIR) {
            find(buf, file);
        } else if (strcmp(de.name, file) == 0) {    //是文件则进行比较, 若与查找一致则输出
            printf("%s\n", buf);
        }
    }
    close(fd);
    return;
}

int main(int argc, char *argv[]) {
    struct stat st;

    if (argc != 3) {
        fprintf(2, "Usage: find dir file\n");
        exit(1);
    }
    // 获取查找目录的状态并判断是否为目录
    if (stat(argv[1], &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", argv[1]);
        exit(2);
    }
    if (st.type != T_DIR) {
        fprintf(2, "find: '%s' is not a directory\n", argv[1]);
        exit(3);
    }
    find(argv[1], argv[2]);
    exit(0);
}
