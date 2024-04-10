#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int pipefd_parent2child[2];    //管道文件描述符，父进程写数据，子进程读数据
    int pipefd_child2parent[2];    //管道文件描述符，子进程写数据，父进程读数据
    int ret = pipe(pipefd_parent2child);
    char buf[2] = {'a', 0};
    if(ret != 0)
    {
        fprintf(2, "create pipe failed\n");
        exit(1);
    }
    ret = pipe(pipefd_child2parent);
    if(ret != 0)
    {
        fprintf(2, "create pipe failed\n");
        exit(2);
    }
    int child_pid = fork();
    if(child_pid < 0)
    {   //进程创建失败
        fprintf(2, "create process failed\n");
        exit(3);
    }
    else if(child_pid > 0)
    {   //父进程
        close(pipefd_parent2child[0]);
        close(pipefd_child2parent[1]);
        write(pipefd_parent2child[1], "a", 1);
        wait((int *)0);
        read(pipefd_child2parent[0], buf, 1);
        fprintf(1, "%d: received pong\n", getpid());
        close(pipefd_parent2child[1]);
        close(pipefd_child2parent[0]);
    }
    else if(child_pid == 0)
    {   //子进程
        close(pipefd_child2parent[0]);
        close(pipefd_parent2child[1]);
        read(pipefd_parent2child[0], buf, 1);
        fprintf(1, "%d: received ping\n", getpid());
        close(pipefd_parent2child[0]);
        write(pipefd_child2parent[1], buf, 1);
        close(pipefd_child2parent[1]);
        exit(0);
    }
    exit(0);
}