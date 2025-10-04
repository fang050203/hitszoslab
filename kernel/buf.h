// 单个缓存块结构
struct buf {
  int valid;                // 数据是否已从磁盘读取
  int disk;                 // 磁盘是否"拥有"此缓冲区
  uint dev;                 // 设备号
  uint blockno;             // 磁盘块号
  struct sleeplock lock;    // 缓存块的睡眠锁
  uint refcnt;              // 引用计数
  struct buf *prev;         // LRU链表前驱
  struct buf *next;         // LRU链表后继
  uchar data[BSIZE];        // 实际数据存储

  int hashid;//新添加的hash映射信息
};
