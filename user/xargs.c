#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXLINE 1024

void run(char* program,char **args)
{
    if(fork()==0)
    {
        exec(program,args);
        exit(0);
    }
    return;
}


int main(int argc,char* argv[])
{
    char buf[MAXLINE]; //读入缓冲区，读管道
    char* p=buf,*last_p=buf; //直接操作指针，在读入的流中安空格等切割参数
    char *xargv[MAXARG]; //新的argv
    char **args=xargv; //参数结构argv里的+管道读入的，args，指向第一个管道读入的
    char *cmd=argv[1]; //命令
    for(int i=1;i<argc;++i)
    {
        *args=argv[i];
        ++args;
    }
    char **pa=args; //填充参数
    while(read(0,p,1)!=0)
    {
        char c=*p;
        if(c==' '||c=='\n') 
        {
            //遇到空格切割，
            *p='\0';
            *(pa++)=last_p;
            last_p=p+1; //下一个参数的开始
            if(c=='\n')
            {
                *pa=0;
                run(cmd,xargv);
                pa=args; //再次回到输入前，即参数目前只有argv传过来的
            }
        }
        p++;
    }
    //最后一行run
    if(pa!=args)
    {
        *p='\0';
        *(pa++)=last_p;
        *pa=0;
        run(cmd,xargv);
    }
    //循环等待所有子进程结束，如果调用者没有子进程，wait 立即返回-1。
    while(wait(0)!=-1){};
    exit(0);

}



// int main(int argc,char **argv)
// {
//     char buf[MAXLINE];
//     char* xargv[MAXARG];
//     int i;
//     int n;
//     int count=0;
//     char *program=argv[1];
//     for(i=1;i<argc;i++)
//     {
//         xargv[count++]=argv[i];
//     }
//     while((n=read(0,buf,MAXLINE))>0) //读入的字节数
//     {
//         if(fork()==0)
//         {
//             char *arg=(char*)malloc(sizeof(buf));
//             int index=0;
//             for(int i=0;i<n;i++)
//             {
//                 if(buf[i]==' '||buf[i]=='\n')
//                 {
//                     arg[index]=0;
//                     xargv[count++]=arg;
//                     index=0;
//                     arg=(char*)malloc(sizeof(buf));
//                 }
//                 else
//                 {
//                     arg[index++]=buf[i];
//                 }
//             }
//             arg[index]=0;
//             xargv[count]=arg;
//             exec(program,xargv);
//         }
//         else
//         {
//             wait((int*)0);
//         }
//     }
//     exit(0);
// }