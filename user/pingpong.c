#include "kernel/types.h"
#include "user.h"

int main(int argc,char*argv[])
{
    int c2f[2];//用于子进程向父进程传递数据
    int f2c[2];//用于父进程向子进程传递数据
    pipe(c2f);//创建管道
    pipe(f2c);

    int pid = fork();
    if(pid <0)//当pid小于0，则说明子进程创建失败
    {
        printf("fork error\n");
        exit(-1);
    }
    else if(pid ==0)//pid等于0，说明为子进程
    {
        int father_pid;//用来从管道中接受父进程的pid
        int child_pid =getpid();//先获取子进程的pid
        close(f2c[1]);//关闭父子管道写端口，避免资源占用
        read(f2c[0],&father_pid,sizeof(int));//向父子管道读端口读入父进程pid
        close(f2c[0]);//关闭父子管道读端口
        printf("%d: received ping from pid %d\n",child_pid,father_pid);//打印输出
        close(c2f[0]);//关闭子父管道读端口，避免资源占用
        write(c2f[1],&child_pid,sizeof(int));//写入子进程pid
        close(c2f[1]);//关闭子进程写入端口
        exit(0);//子进程退出
    } else//为父进程
    {
        int father_pid=getpid();//获取父进程的pid
        int child_pid;//用来保存返回的子进程pid
        close(f2c[0]);//关闭父子管道读端口，避免资源占用
        write(f2c[1],&father_pid,sizeof(int));//向父子管道写端口写入父进程pid
        close(f2c[1]);//关闭父子管道写端口
        close(c2f[1]);//关闭子父端口写端口
        read(c2f[0],&child_pid,sizeof(int));//从子父管道中读出子进程pid
        close(c2f[0]);//关闭子父管道读端口
        printf("%d: received pong from pid %d\n",father_pid,child_pid);//打印输出
        wait(0);//等待子进程退出
        exit(0);//父进程退出
    }
}