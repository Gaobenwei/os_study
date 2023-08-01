#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc,char* argv[])
{
    //int i;
    if(argc < 2){
        fprintf(2, "please input time\n");
        exit(1);
    }
    int n=atoi(argv[1]);
    sleep(n);
    exit(0);
}