#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 格式化文件或目录的名称
char* fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // 寻找最后一个斜杠后的第一个字符
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)]=0;
  return buf;
}

void find(char *path,char *filename)
{
    char buf[512],*p;
    int fd;
    struct dirent de;
    struct stat st;

    // 打开给定的文件或目录
    if((fd = open(path, 0)) < 0){
        fprintf(2, "ls: 无法打开 %s\n", path);
        return;
    }

    // 获取文件的状态信息
    if(fstat(fd, &st) < 0){
        fprintf(2, "ls: 无法获取状态信息 %s\n", path);
        close(fd);
        return;
    }

    //检查path类型
    switch(st.type)
    {
    case T_FILE:
        //是文件名称，直接对比
        if(strcmp(fmtname(path),filename)==0)
            printf("%s\n",path);
        break;

    case T_DIR:
        //目录
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
            printf("ls: 路径太长\n");
            break;
        }
        //add '/'
        strcpy(buf,path);
        p=buf+strlen(buf);
        *p++='/';
        while(read(fd,&de,sizeof(de))==sizeof(de))
        {
            if(de.inum==0)
                continue;
            memmove(p,de.name,DIRSIZ); //将其中的名称部分（de.name）复制到 buf 中的当前位置 p 处
            p[DIRSIZ]=0;

            //不用查找. ..
            if(!strcmp(de.name,".")||!strcmp(de.name,".."))
                continue;
            if(stat(buf, &st) < 0){
                printf("ls: 无法获取状态信息 %s\n", buf);
                continue;
            }
            //递归
            
            find(buf,filename);
            

        }
        break;
    }

    close(fd);  
}

int main(int argc,char* argv[])
{
    if(argc!=3)
    {
        printf("pleasefind: find <path> <fileName>\n");
        exit(0);
    }
    find(argv[1],argv[2]);
    exit(0);
}