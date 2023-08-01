#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

//主进程：生成 n ∈ [2,35] -> 子进程1：筛掉所有 2 的倍数 -> 子进程2：筛掉所有 3 的倍数 -> 子进程3：筛掉所有 5 的倍数 -> .....
// 一次 sieve 调用是一个筛子阶段，会从 pleft 获取并输出一个素数 p，筛除 p 的所有倍数
// 同时创建下一 stage 的进程以及相应输入管道 pright，然后将剩下的数传到下一 stage 处理

void sieve(int pleft[2])
{
    int num_left;
    read(pleft[0],&num_left,sizeof(num_left));
    if(num_left==-1) //到了结尾
    {
        exit(0);
    }
    printf("prime %d\n",num_left);
    int pright[2];
	pipe(pright); // 创建用于输出到下一 stage 的进程的输出管道 pright

    // int num_[35];
    // int num;
    if(fork()==0)
    {
        close(pright[1]);
        close(pleft[0]);
        sieve(pright);
    }
    else
    {
        close(pright[0]);
        // for(int i=0;;)
        // {
        //     read(pleft[0],&num,sizeof(num));
        //     if(num%num_left!=0)
        //     {
        //         num_[i++]=num;
        //     }
        //     if(num==-1)
        //     {
        //         num_[i++]=num;
        //         break;
        //     }
        // }
        int buf;
        while(read(pleft[0],&buf,sizeof(buf))&& buf!=-1)
        {
            if(buf%num_left!=0)
            {
                write(pright[1],&buf,sizeof(buf));
            }
        }
        buf=-1;
        write(pright[1],&buf,sizeof(buf));
        // for(int i=0;;)
        // {
        //     write(pright[1],&num_[i],sizeof(int));
        //     if(num_[i]==-1)
        //         break;
        // }
        wait(0);
        exit(0);
    }
    

}

int main(int argc,char* argv[])
{
    int pipe_[2];
    pipe(pipe_); //生成管道，用来输入2--35
    if(fork()==0) //第一个子进程
    {
        //第一段的stage,只从左端输入
        close(pipe_[1]);
        sieve(pipe_);
        exit(0);
    }
    else //主进程
    {
        close(pipe_[0]); //最左边进程，不读，只写
        int i;
        for(i=2;i<=35;++i)
        {
            write(pipe_[1],&i,sizeof(i));
        }
        i=-1; //结尾标志
        write(pipe_[1],&i,sizeof(i));
        wait(0);
    exit(0);
    }
    

}