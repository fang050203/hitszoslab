b *0x80000fb2
#在main函数处加上断点
c
b main.c:42
c
si 2
#进入scheduler函数
n 9

si 33
#进入forkret函数


n 5
s
#进入usertrapret函数


p cpus[$tp]->proc->name
#打印第一个initcode进程

u 112


si 13


si 36
# 执行inicode代码
si 6
#进入ecall


si 40
#进入usertrap函数

n 14

#执行完syscall函数


p cpus[$tp]->proc->name 
#此时进程已经切换为init
s
da