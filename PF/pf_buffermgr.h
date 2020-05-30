// File:        pf_buffermgr.h
// Description: PF_BufferMgr class interface


#ifndef PF_BUFFERMGR_H
#define PF_BUFFERMGR_H

#include "pf_internal.h"
#include "pf_hashtable.h"

// 无效槽号
#define INVALID_SLOT  (-1)

//
// PF_BufPageDesc - struct containing data about a page in the buffer
//
struct PF_BufPageDesc {
    SlotNum    next;        // next in the linked list of buffer pages
    SlotNum    prev;        // prev in the linked list of buffer pages
    int        bDirty;      // TRUE if page is dirty
    short int  pinCount;    // pin count
    PageNum    pageNum;     // page number for this page
    int        fd;          // OS file descriptor of this page
    char       *pPage;      // 这个页的数据内容，大小为4KB，包括页的首部信息
};

//
// PF_BufferMgr - manage the page buffer
//
class PF_BufferMgr {
public:

    PF_BufferMgr     (int numPages);             // Constructor - allocate

    ~PF_BufferMgr    ();                         // Destructor

    //根据文件描述符、页号、从缓冲区中取得这个页，ppBuffer是指向*pPage的指针
    // 若缓冲区中不存在这个页，要从磁盘上先将这个页读取至缓冲区
    RC  GetPage      (int fd, PageNum pageNum, char **ppBuffer,
                      int bMultiplePins = TRUE);

    // 在缓冲区中为某个页号的页分配一块空间
    // *ppBuffer是这个4KB空间的地址
    // 在需要增加某个文件的页数时，可以先在缓冲区中分配也一个页
    RC  AllocatePage (int fd, PageNum pageNum, char **ppBuffer);

    // 标记为脏页
    RC  MarkDirty    (int fd, PageNum pageNum);

    // Unpin页
    RC  UnpinPage    (int fd, PageNum pageNum);

    // 将fd文件所有的页写回磁盘，并释放缓冲区的空间
    RC  FlushPages   (int fd);

    // 将fd,页号为pageNum的页回写磁盘，但并不从缓冲区中消失，
    RC ForcePages    (int fd, PageNum pageNum);


    // Remove all entries from the Buffer Manager.
    RC ClearBuffer  ();
    // Display all entries in the buffer
    RC PrintBuffer   ();

    // Attempts to resize the buffer to the new size
    RC ResizeBuffer  (int iNewSize);

    // Three Methods for manipulating raw memory buffers.  These memory
    // locations are handled by the buffer manager, but are not
    // associated with a particular file.  These should be used if you
    // want memory that is bounded by the size of the buffer pool.

    // Return the size of the block that can be allocated.
    RC GetBlockSize  (int &length) const;

    // Allocate a memory chunk that lives in buffer manager
    RC AllocateBlock (char *&buffer);
    // Dispose of a memory chunk managed by the buffer manager.
    RC DisposeBlock  (const char *buffer);

private:
    RC  InsertFree   (SlotNum slot);                 // 把slot插入到空闲链表头部
    RC  LinkHead     (SlotNum slot);                 // 将slot槽插入到使用槽链表的表头
    RC  Unlink       (SlotNum slot);                 // 将slot解链，就是链接slot两边的链
    RC  InternalAlloc(SlotNum &slot);                // 分配一个可用的空闲槽的槽号

    // 从磁盘上根据文件描述符和页号，将真个页的数据(4KB)加载进缓冲区，dest==pPage;
    RC  ReadPage     (int fd, PageNum pageNum, char *dest);

    // 将source指向的缓冲区地址的4KB数据写入磁盘
    RC  WritePage    (int fd, PageNum pageNum, char *source);

    // Init the page desc entry
    RC  InitPageDesc (int fd, PageNum pageNum, SlotNum slot);

    PF_BufPageDesc *bufTable;                     // info on buffer pages
    PF_HashTable   hashTable;                     // Hash table object
    int            numPages;                      // # of pages in the buffer
    int            pageSize;                      // Size of pages in the buffer
    SlotNum        first;                         // MRU page slot
    SlotNum        last;                          // LRU page slot
    SlotNum        free;                          // head of free list
};

#endif
