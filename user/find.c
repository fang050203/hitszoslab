#include "kernel/types.h"//注意头文件顺序，这个头文件一定是第一个
#include "kernel/stat.h"//引入头文件
#include "user.h"
#include "kernel/fs.h"


char *fmtname(char *path) {//从ls.c拿来的函数，先直接用
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}



void find(char *path,char *filename)
{
    char buf[512];//处理缓冲区
    char *p;//主要用来对buf进行处理的指针操作
    int fd;//用来存储文件描述符
    struct dirent de;
    struct stat st;
    //filename填充，空位处要用空格填充，避免比较字符串失败
    char f[DIRSIZ];
    memmove(f, filename, strlen(filename));
    memset(f + strlen(filename), ' ', DIRSIZ - strlen(filename)); 

    fd=open(path,0);
    if(fd<0)//当文件打开异常处理
    {
        printf("open error\n");
        return;
    }
    if(fstat(fd,&st)<0)
    {//当文件状态异常
        printf("stat error\n");
        close(fd);//关闭文件描述符，避免长时间系统占用
        return;
    }
    switch(st.type){
        case T_FILE:
        //对于文件夹也需要看是否满足条件
        if(!strcmp(f,fmtname(path)))
        {
            printf("%s\n",path);
        }
        break;

        case T_DIR:
        //有一段异常检测，暂时先不加进去
        //可能需要加入
        //if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        //printf("path too long\n");
        //break;
      //}


        //这一段的主要作用是遍历目录时提供文件地址
        strcpy(buf,path);
        p=buf+strlen(buf);
        *p++='/';
        if(!strcmp(f,fmtname(path)))
        {
            printf("%s\n",path);
        }
        while(read(fd,&de,sizeof(de))==sizeof(de))
        {
            if(de.inum==0 || !strcmp(de.name,".") || !strcmp(de.name,".."))continue;//当条目无效时直接选择跳过
            memmove(p,de.name,DIRSIZ);//将文件名加入buf的末尾
            p[DIRSIZ]=0;//末尾b补齐0
            find(buf,filename);
        }
        break;
    }
    close(fd);//关闭文件描述符，避免资源占用
    return;
}
int main(int argc,char *argv[])
{
    if(argc!=3)
    {
        //当输入参数错误，处理不正当输入
        printf("input error\n");
        exit(-1);
    }

    find(argv[1],argv[2]);
    exit(0);
}