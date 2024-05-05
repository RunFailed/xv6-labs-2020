struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  //struct buf *prev; // LRU cache list     //Lab8:2
  struct buf *next;
  uchar data[BSIZE];
  uint tick;    //最近一次使用时的时间戳   //Lab8:2
};

