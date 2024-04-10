#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int subprocess(int old_pipefd[])
{
    close(old_pipefd[1]);
    int prime = 0;
    int num = 0;
    int new_pipefd[2];
    if(read(old_pipefd[0], &prime, 4))
    {
        pipe(new_pipefd);
        printf("prime %d\n", prime);
        if(fork() == 0)
        {
            close(old_pipefd[0]);
            subprocess(new_pipefd);
        }
        else
        {
            close(new_pipefd[0]);
            while(read(old_pipefd[0], &num, 4))
            {
                if(num % prime == 0) continue;
                write(new_pipefd[1], &num, 4);
            }
            close(new_pipefd[1]);
            close(old_pipefd[0]);
            wait((int *)0);
        }
    }
    else
    {
        close(old_pipefd[0]);
    }
    exit(0);
}

int main(int argc, char* argv[])
{
    int old_pipefd[2];
    pipe(old_pipefd);
    if(fork() == 0)
    {   //子进程
        subprocess(old_pipefd);
    }
    else
    {   //父进程
        close(old_pipefd[0]);
        for(int i = 2; i <= 35; i++)
        {
            write(old_pipefd[1], &i, 4);
        }
        close(old_pipefd[1]);
        wait((int *)0);
    }
    exit(0);
}