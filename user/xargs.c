#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

#define MAXLEN 32   //每个参数的最大长度

int main(int argc, char* argv[])
{
    char* command = "echo";     //存放指令，默认为echo
    char* parameters[MAXARG];
    char paramsBuf[MAXLEN * (MAXARG + 1)] = {0};
    int oriParamNum = 0;
    if(argc > 1)
    {
        command = argv[1];
        for(int i = 1; i < argc; i++) {
            parameters[oriParamNum++] = argv[i];
        }
    }
    else
    {
        parameters[oriParamNum++] = command;
    }
    int paramIdx = oriParamNum;
    char* ptr = paramsBuf;
    int paramLen = 0;
    while(1)
    {
        int res = read(0, ptr, 1);
        char c = *ptr;
        if(res != 0 && c != ' ' && c != '\n')
        {
            paramLen++;
            ptr++;
            continue;
        }
        parameters[paramIdx++] = ptr - paramLen;
        paramLen = 0;
        *ptr = 0;
        ptr++;
        if(paramIdx >= MAXARG && c == ' ')
        {
            while(read(0, &c, 1))
            {
                if(c == '\n') break;
            }
        }
        if(c != ' ')
        {
            if(fork() == 0)
            {
                exec(command, parameters);
                exit(0);
            }
            else
            {
                wait((int *)0);
                paramIdx = oriParamNum;
                ptr = paramsBuf;
            }
        }
        if(res == 0) break;
    }
    exit(0);
}