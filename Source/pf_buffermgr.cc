// File:        pf_buffermgr.cc
// Description: PF_BufferMgr class implementation


#include <cstdio>
#include <unistd.h>
#include <iostream>
#include "../Header/pf_buffermgr.h"

using namespace std;

// The switch PF_STATS indicates that the user wishes to have statistics
// tracked for the PF layer
#ifdef PF_STATS
#include "statistics.h"   // For StatisticsMgr interface

// Global variable for the statistics manager
StatisticsMgr *pStatisticsMgr;
#endif

#ifdef PF_LOG

//
// WriteLog
//
// This is a self contained unit that will create a new log file and send
// psMessage to the log file.  Notice that I do not close the file fLog at
// any time.  Hopefully if all goes well this will be done when the program
// exits.
//
void WriteLog(const char *psMessage)
{
   static FILE *fLog = NULL;

   // The first time through we have to create a new Log file
   if (fLog == NULL) {
      // This is the first time so I need to create a new log file.
      // The log file will be named "PF_LOG.x" where x is the next
      // available sequential number
      int iLogNum = -1;
      int bFound = FALSE;
      char psFileName[10];

      while (iLogNum < 999 && bFound==FALSE) {
         iLogNum++;
         sprintf (psFileName, "PF_LOG.%d", iLogNum);
         fLog = fopen(psFileName,"r");
         if (fLog==NULL) {
            bFound = TRUE;
            fLog = fopen(psFileName,"w");
         } else
            delete fLog;
      }

      if (!bFound) {
         cerr << "Cannot create a new log file!\n";
         exit(1);
      }
   }
   // Now we have the log file open and ready for writing
   fprintf (fLog, psMessage);
}
#endif


//
// PF_BufferMgr
//
// Desc: Constructor - called by PF_Manager::PF_Manager
//       The buffer manager manages the page buffer.  When asked for a page,
//       it checks if it is in the buffer.  If so, it pins the page (pages
//       can be pinned multiple times).  If not, it reads it from the file
//       and pins it.  If the buffer is full and a new page needs to be
//       inserted, an unpinned page is replaced according to an LRU
// In:   numPages - the number of pages in the buffer
//
// Note: The constructor will initialize the global pStatisticsMgr.  We
//       make it global so that other components may use it and to allow
//       easy access.
//
// Aut2003
// numPages changed to _numPages for to eliminate CC warnings

PF_BufferMgr::PF_BufferMgr(int _numPages) : hashTable(PF_HASH_TBL_SIZE)
{
    // Initialize local variables
    this->numPages = _numPages;
    pageSize = PF_PAGE_SIZE + sizeof(PF_PageHdr);

#ifdef PF_STATS
    // Initialize the global variable for the statistics manager
   pStatisticsMgr = new StatisticsMgr();
#endif

#ifdef PF_LOG
    char psMessage[100];
   sprintf (psMessage, "Creating buffer manager. %d pages of size %d.\n",
         numPages, PF_PAGE_SIZE+sizeof(PF_PageHdr));
   WriteLog(psMessage);
#endif

    // Allocate memory for buffer page description table
    bufTable = new PF_BufPageDesc[numPages];

    // Initialize the buffer table and allocate memory for buffer pages.
    // Initially, the free list contains all pages
    for (int i = 0; i < numPages; i++) {
        if ((bufTable[i].pPage = new char[pageSize]) == NULL) {
            cerr << "Not enough memory for buffer\n";
            exit(1);
        }

        memset ((void *)bufTable[i].pPage, 0, pageSize);

        bufTable[i].prev = i - 1;
        bufTable[i].next = i + 1;
    }
    bufTable[0].prev = bufTable[numPages - 1].next = INVALID_SLOT;
    free = 0;
    first = last = INVALID_SLOT;

#ifdef PF_LOG
    WriteLog("Succesfully created the buffer manager.\n");
#endif
}

//
// ~PF_BufferMgr
//
// Desc: Destructor - called by PF_Manager::~PF_Manager
//
PF_BufferMgr::~PF_BufferMgr()
{
    // Free up buffer pages and tables
    for (int i = 0; i < this->numPages; i++)
        delete [] bufTable[i].pPage;

    delete [] bufTable;

#ifdef PF_STATS
    // Destroy the global statistics manager
   delete pStatisticsMgr;
#endif

#ifdef PF_LOG
    WriteLog("Destroyed the buffer manager.\n");
#endif
}

/**
 * 得到fd/pageNum的页
 * 1.首先通过哈希桶查找，看看这个页是否在缓冲区中
 *     --在缓冲区中，返回pPage给ppbBuffer
 *     --不在缓冲区中，分配一个槽号，并从磁盘中读取这个页，再返回为ppBuffer
 * @param fd                文件描述符
 * @param pageNum           页号
 * @param ppBuffer          指向*pPage的指针
 * @param bMultiplePins     这个页是否允许多个进程访问
 * @return
 */
RC PF_BufferMgr::GetPage(int fd, PageNum pageNum, char **ppBuffer,
                         int bMultiplePins)
{
    RC  rc;     // return code
    int slot;   // 槽号

#ifdef PF_LOG
    char psMessage[100];
   sprintf (psMessage, "Looking for (%d,%d).\n", fd, pageNum);
   WriteLog(psMessage);
#endif


#ifdef PF_STATS
    pStatisticsMgr->Register(PF_GETPAGE, STAT_ADDONE);
#endif

    // 在哈希桶中查找这个页的槽号
    if ((rc = hashTable.Find(fd, pageNum, slot)) &&
        (rc != PF_HASHNOTFOUND))
        return (rc);                // unexpected error

    // 如果没有找到，分配一个槽，先从磁盘上读取这个页
    if (rc == PF_HASHNOTFOUND) {

#ifdef PF_STATS
        pStatisticsMgr->Register(PF_PAGENOTFOUND, STAT_ADDONE);
#endif
        // 分配一个可用的槽号
        if ((rc = InternalAlloc(slot)))
            return (rc);

        // 从磁盘上读取这个页，更新哈希桶
        if ((rc = ReadPage(fd, pageNum, bufTable[slot].pPage)) ||
            (rc = hashTable.Insert(fd, pageNum, slot)) ||
            (rc = InitPageDesc(fd, pageNum, slot))) {
            // InitPageDesc会将pinCount设置为1

            // 若出事，需要将这个槽放入到空闲链表,健壮！
            Unlink(slot);
            InsertFree(slot);
            return rc;
        }
    }else{
        //GetPage方法会导致pinCount计数器加一


        // Error if we don't want to get a pinned page
        if (!bMultiplePins && bufTable[slot].pinCount > 0)
            return (PF_PAGEPINNED);

        // Page is alredy in memory, just increment pin count
        bufTable[slot].pinCount++;
    }
    // 标记这个槽为最近使用的槽
    if ((rc = Unlink(slot)) ||
        (rc = LinkHead (slot)))
        return rc;

    *ppBuffer = bufTable[slot].pPage;

    return OK_RC;
}

/**
 * 为fd/pageNum的页在缓冲区中分配一个空间
 * 当对需要对fd这个文件的内容追加更多的页时，需要调用该方法
 * 在缓冲区中分配一个新页来追加记录
 * @param fd           文件描述符
 * @param pageNum      新页的页号
 * @param ppBuffer     新页的地址
 * @return
 */
RC PF_BufferMgr::AllocatePage(int fd, PageNum pageNum, char **ppBuffer)
{
    RC  rc;     // return code
    int slot;   // buffer slot where page is located

#ifdef PF_LOG
    char psMessage[100];
   sprintf (psMessage, "Allocating a page for (%d,%d)....", fd, pageNum);
   WriteLog(psMessage);
#endif

    // 首先检查页号是否已经存在，若存在就返回错误
    if (!(rc = hashTable.Find(fd, pageNum, slot)))
        return PF_PAGEINBUF;
    else if (rc != PF_HASHNOTFOUND)
        return rc;

    // 分配一个可用的槽
    if ((rc = InternalAlloc(slot)))
        return (rc);
    // 插入哈希桶
    if ((rc = hashTable.Insert(fd, pageNum, slot)) ||
        (rc = InitPageDesc(fd, pageNum, slot))) {
        Unlink(slot);
        InsertFree(slot);
        return (rc);
    }

#ifdef PF_LOG
    WriteLog("Succesfully allocated page.\n");
#endif

    // 得到这个页空间的地址
    *ppBuffer = bufTable[slot].pPage;

    return OK_RC;
}

/**
 * 标记一个页为脏页
 * 当调用GetPage方法时需要调用该方法
 * 检查该页是否存在，不存在返回错误
 * 存在，就找到它的槽号，并将bDirty标志位设置为TRUE
 * @param fd
 * @param pageNum
 * @return
 */
RC PF_BufferMgr::MarkDirty(int fd, PageNum pageNum)
{
    RC  rc;       // return code
    int slot;     // buffer slot where page is located

#ifdef PF_LOG
    char psMessage[100];
   sprintf (psMessage, "Marking dirty (%d,%d).\n", fd, pageNum);
   WriteLog(psMessage);
#endif

    // The page must be found and pinned in the buffer
    if ((rc = hashTable.Find(fd, pageNum, slot))){
        if ((rc == PF_HASHNOTFOUND))
            return (PF_PAGENOTINBUF);
        else
            return (rc);              // unexpected error
    }

    // 必须是正在操作的页才可以被标记为脏页
    if (bufTable[slot].pinCount == 0)
        return (PF_PAGEUNPINNED);

    // Mark this page dirty
    bufTable[slot].bDirty = TRUE;

    if ((rc = Unlink(slot)) || (rc = LinkHead (slot)))
        return rc;

    return OK_RC;
}

/**
 * unpin一个页,使用GetPage会使pinCount+1,所以在GetThisPage的函数结束前，需要unpin页
 * 当结束一个页的操作时要调用该方法，将页的pinCount计数减1
 * 若pinCount计数值==0,表示有个进程刚刚使用完这个页，把它放到使用页的表头
 * @param fd        文件描述符号
 * @param pageNum   页号
 * @return
 */
RC PF_BufferMgr::UnpinPage(int fd, PageNum pageNum)
{
    RC  rc;
    int slot;

    // 检查这个页是否在缓冲区中，不在返回错误
    if ((rc = hashTable.Find(fd, pageNum, slot))){
        if ((rc == PF_HASHNOTFOUND))
            return (PF_PAGENOTINBUF);
        else
            return (rc);
    }

    if (bufTable[slot].pinCount == 0)
        return (PF_PAGEUNPINNED);

#ifdef PF_LOG
    char psMessage[100];
   sprintf (psMessage, "Unpinning (%d,%d). %d Pin count\n",
         fd, pageNum, bufTable[slot].pinCount-1);
   WriteLog(psMessage);
#endif

    // 如果pinCount减到0
    if (--(bufTable[slot].pinCount) == 0) {
        if ((rc = Unlink(slot)) ||
            (rc = LinkHead(slot)))
            return (rc);
    }

    // Return ok
    return OK_RC;
}

/**
 * 将fd的所有页都回写磁盘
 * 使用此函数前应当确保，fd的所有页的pinCount==0
 * @param fd 文件描述符
 * @return
 */
RC PF_BufferMgr::FlushPages(int fd)
{
    RC rc, rcWarn = OK_RC;

    // 线性扫描所有页
    int slot = first;
    while (slot != INVALID_SLOT) {
        int next = bufTable[slot].next;
        // If the page belongs to the passed-in file descriptor
        if (bufTable[slot].fd == fd) {
            // 确保这个页没有被pin
            if (bufTable[slot].pinCount) {
                rcWarn = PF_PAGEPINNED;
            }
            else {
                // 回写脏页
                if (bufTable[slot].bDirty) {
                    if ((rc = WritePage(fd, bufTable[slot].pageNum, bufTable[slot].pPage)))
                        return (rc);
                    bufTable[slot].bDirty = FALSE;
                }

                // 将这个页从哈希桶中移除并加入到空闲链
                if ((rc = hashTable.Delete(fd, bufTable[slot].pageNum)) ||
                    (rc = Unlink(slot)) ||
                    (rc = InsertFree(slot)))  // 只有被回写过的槽，说明这个槽中的数据已经被保存了
                                              // 槽的资源应该被释放
                                              // 才会被加入到空闲页链表
                    return (rc);
            }
        }
        slot = next;
    }

    return (rcWarn);
}

/**
 * 将fd/pageNum的页(若为脏页)回写磁盘，但该槽并不从缓冲区中释放，将相关信息重置
 * 使用该函数前，请确保该页的pinCount为0，
 * 如果pageNum == ALL_PAGES,就将所有的页都回写一下磁盘
 * @param fd
 * @param pageNum
 * @return
 */
RC PF_BufferMgr::ForcePages(int fd, PageNum pageNum)
{
    RC rc;  // return codes

#ifdef PF_LOG
    char psMessage[100];
   sprintf (psMessage, "Forcing page %d for (%d).\n", pageNum, fd);
   WriteLog(psMessage);
#endif

   if(pageNum != ALL_PAGES){
       // 将某一个页回写磁盘
       // 检查这个页是否在缓冲区中
       SlotNum slot;
       if((rc = hashTable.Find(fd,pageNum,slot)))
           return rc;
       // 如果是脏页并且pinCount=0就回写，否则就啥都不做
       if(bufTable[slot].pinCount != 0)
           return PF_PAGEPINNED;
       if (bufTable[slot].bDirty) {
           if ((rc = WritePage(fd, bufTable[slot].pageNum, bufTable[slot].pPage)))
               return (rc);
           bufTable[slot].bDirty = FALSE;
       }
   } else{
       // 将该文件的所有页都回写
       int slot = first;         // 将空闲页链表中的页全部回写
       while(slot != INVALID_SLOT){
           if(bufTable[slot].fd == fd){
               if(bufTable[slot].pinCount != 0)
                   return PF_PAGEPINNED;
               if (bufTable[slot].bDirty) {
                   if ((rc = WritePage(fd, bufTable[slot].pageNum, bufTable[slot].pPage)))
                       return (rc);
                    bufTable[slot].bDirty = FALSE;
               }
           }
           slot = bufTable[slot].next;
       }
   }
    return OK_RC;
}


//
// PrintBuffer
//
// Desc: Display all of the pages within the buffer.
//       This routine will be called via the system command.
// In:   Nothing
// Out:  Nothing
// Ret:  Always returns 0
//
RC PF_BufferMgr::PrintBuffer()
{
    cout << "Buffer contains " << numPages << " pages of size "
         << pageSize <<".\n";
    cout << "Contents in order from most recently used to "
         << "least recently used.\n";

    int slot, next;
    slot = first;
    while (slot != INVALID_SLOT) {
        next = bufTable[slot].next;
        cout << slot << " :: \n";
        cout << "  fd = " << bufTable[slot].fd << "\n";
        cout << "  pageNum = " << bufTable[slot].pageNum << "\n";
        cout << "  bDirty = " << bufTable[slot].bDirty << "\n";
        cout << "  pinCount = " << bufTable[slot].pinCount << "\n";
        slot = next;
    }

    if (first==INVALID_SLOT)
        cout << "Buffer is empty!\n";
    else
        cout << "All remaining slots are free.\n";

    return 0;
}


//
// ClearBuffer
//
// Desc: Remove all entries from the buffer manager.
//       This routine will be called via the system command and is only
//       really useful if the user wants to run some performance
//       comparison starting with an clean buffer.
// In:   Nothing
// Out:  Nothing
// Ret:  Will return an error if a page is pinned and the Clear routine
//       is called.
RC PF_BufferMgr::ClearBuffer()
{
    RC rc;

    int slot, next;
    slot = first;
    while (slot != INVALID_SLOT) {
        next = bufTable[slot].next;
        if (bufTable[slot].pinCount == 0)
            if ((rc = hashTable.Delete(bufTable[slot].fd,
                                       bufTable[slot].pageNum)) ||
                (rc = Unlink(slot)) ||
                (rc = InsertFree(slot)))
                return (rc);
        slot = next;
    }

    return 0;
}

//
// ResizeBuffer
//
// Desc: Resizes the buffer manager to the size passed in.
//       This routine will be called via the system command.
// In:   The new buffer size
// Out:  Nothing
// Ret:  0 for success or,
//       Some other PF error (probably PF_NOBUF)
//
// Notes: This method attempts to copy all the old pages which I am
// unable to kick out of the old buffer manager into the new buffer
// manager.  This obviously cannot always be successfull!
//
RC PF_BufferMgr::ResizeBuffer(int iNewSize)
{
    int i;
    RC rc;

    // First try and clear out the old buffer!
    ClearBuffer();

    // Allocate memory for a new buffer table
    PF_BufPageDesc *pNewBufTable = new PF_BufPageDesc[iNewSize];

    // Initialize the new buffer table and allocate memory for buffer
    // pages.  Initially, the free list contains all pages
    for (i = 0; i < iNewSize; i++) {
        if ((pNewBufTable[i].pPage = new char[pageSize]) == NULL) {
            cerr << "Not enough memory for buffer\n";
            exit(1);
        }

        memset ((void *)pNewBufTable[i].pPage, 0, pageSize);

        pNewBufTable[i].prev = i - 1;
        pNewBufTable[i].next = i + 1;
    }
    pNewBufTable[0].prev = pNewBufTable[iNewSize - 1].next = INVALID_SLOT;

    // Now we must remember the old first and last slots and (of course)
    // the buffer table itself.  Then we use insert methods to insert
    // each of the entries into the new buffertable
    int oldFirst = first;
    PF_BufPageDesc *pOldBufTable = bufTable;

    // Setup the new number of pages,  first, last and free
    numPages = iNewSize;
    first = last = INVALID_SLOT;
    free = 0;

    // Setup the new buffer table
    bufTable = pNewBufTable;

    // We must first remove from the hashtable any possible entries
    int slot, next, newSlot;
    slot = oldFirst;
    while (slot != INVALID_SLOT) {
        next = pOldBufTable[slot].next;

        // Must remove the entry from the hashtable from the
        if ((rc=hashTable.Delete(pOldBufTable[slot].fd, pOldBufTable[slot].pageNum)))
            return (rc);
        slot = next;
    }

    // Now we traverse through the old buffer table and copy any old
    // entries into the new one
    slot = oldFirst;
    while (slot != INVALID_SLOT) {

        next = pOldBufTable[slot].next;
        // Allocate a new slot for the old page
        if ((rc = InternalAlloc(newSlot)))
            return (rc);

        // Insert the page into the hash table,
        // and initialize the page description entry
        if ((rc = hashTable.Insert(pOldBufTable[slot].fd,
                                   pOldBufTable[slot].pageNum, newSlot)) ||
            (rc = InitPageDesc(pOldBufTable[slot].fd,
                               pOldBufTable[slot].pageNum, newSlot)))
            return (rc);

        // Put the slot back on the free list before returning the error
        Unlink(newSlot);
        InsertFree(newSlot);

        slot = next;
    }

    // Finally, delete the old buffer table
    delete [] pOldBufTable;

    return 0;
}

/**
 * 将slot的槽放入空闲链的首部以方便下次取用
 * @param slot 空闲的槽
 * @return
 */
RC PF_BufferMgr::InsertFree(SlotNum slot)
{
    // 检查
    if(slot < 0)
        return PF_INVALIDSLOT;

    bufTable[slot].next = free;
    free = slot;

    // Return ok
    return OK_RC;
}

/**
 * 将slot槽插入到使用槽链表的表头
 * @param slot
 * @return
 */
RC PF_BufferMgr::LinkHead(SlotNum slot)
{
    // Set next and prev pointers of slot entry
    bufTable[slot].next = first;
    bufTable[slot].prev = INVALID_SLOT;

    // If list isn't empty, point old first back to slot
    if (first != INVALID_SLOT)
        bufTable[first].prev = slot;

    first = slot;

    // if list was empty, set last to slot
    if (last == INVALID_SLOT)
        last = first;

    // Return ok
    return OK_RC;
}

//
// Unlink
//
// Desc: Internal.  Unlink the slot from the used list.  Assume that
//       slot is valid.  Set prev and next pointers to INVALID_SLOT.
//       The caller is responsible to either place the unlinked page into
//       the free list or the used list.
// In:   slot - slot number to unlink
// Ret:  PF return code
//
RC PF_BufferMgr::Unlink(SlotNum slot)
{
    // If slot is at head of list, set first to next element
    if (first == slot)
        first = bufTable[slot].next;

    // If slot is at end of list, set last to previous element
    if (last == slot)
        last = bufTable[slot].prev;

    // If slot not at end of list, point next back to previous
    if (bufTable[slot].next != INVALID_SLOT)
        bufTable[bufTable[slot].next].prev = bufTable[slot].prev;

    // If slot not at head of list, point prev forward to next
    if (bufTable[slot].prev != INVALID_SLOT)
        bufTable[bufTable[slot].prev].next = bufTable[slot].next;

    // Set next and prev pointers of slot entry
    bufTable[slot].prev = bufTable[slot].next = INVALID_SLOT;

    // Return ok
    return OK_RC;
}

/**
 * 分配一个可用的槽号
 * free是缓冲区中第一个空闲槽的槽号
 * 若free不为NVALID_SLOT，则使用这个槽号
 * 若free为NVALID_SLOT，说明没有空闲槽，
 *      需要将一个缓冲区中不在使用中的页回写进磁盘
 * @param slot
 * @return
 */
RC PF_BufferMgr::InternalAlloc(SlotNum &slot)
{
    RC  rc;       // return code

    // free 是可用的槽，将该槽的槽号返回，并重新设置free的值
    if (free != INVALID_SLOT) {
        slot = free;
        free = bufTable[slot].next;
    }
    else {

        // 从last(最久未使用的页)开始向前搜索，找到一个不被pin的槽
        for (slot = last; slot != INVALID_SLOT; slot = bufTable[slot].prev) {
            if (bufTable[slot].pinCount == 0)
                break;
        }

        // 所有的槽都在被使用，返回缓冲区满
        if (slot == INVALID_SLOT)
            return (PF_NOBUF);

        // 如果该页是脏页，就调用writePage,否则不需要
        if (bufTable[slot].bDirty) {
            if ((rc = WritePage(bufTable[slot].fd, bufTable[slot].pageNum,
                                bufTable[slot].pPage)))
                return (rc);

            bufTable[slot].bDirty = FALSE;
        }

        // 从哈希桶中释放这个页
        if ((rc = hashTable.Delete(bufTable[slot].fd, bufTable[slot].pageNum)) ||
            (rc = Unlink(slot)))
            return (rc);
    }

    // 解开这个节点，并将这个槽添加到使用链表的表头
    if ((rc = Unlink(slot)) || (rc = LinkHead(slot)))
        return (rc);

    return OK_RC;
}

/**
 * 从磁盘上读取一个页(4KB)到缓冲区(dest)的位置
 * @param fd        文件描述符号
 * @param pageNum   页号
 * @param dest      目标地址
 * @return
 */
RC PF_BufferMgr::ReadPage(int fd, PageNum pageNum, char *dest)
{

#ifdef PF_LOG
    char psMessage[100];
   sprintf (psMessage, "Reading (%d,%d).\n", fd, pageNum);
   WriteLog(psMessage);
#endif

#ifdef PF_STATS
    pStatisticsMgr->Register(PF_READPAGE, STAT_ADDONE);
#endif
    //检查
    if(dest == NULL)
        return PF_NOBUF;

    // 根据页号和页大小计算偏移量
    long offset = pageNum * (long)pageSize + PF_FILE_HDR_SIZE;
    if (lseek(fd, offset, L_SET) < 0)
        return (PF_UNIX);

    // 读取数据
    int numBytes = read(fd, dest, pageSize);
    if (numBytes < 0)
        return (PF_UNIX);
    else if (numBytes != pageSize)
        return (PF_INCOMPLETEREAD);
    else
        return OK_RC;
}

/**
 * 根据文件描述符和页号，将source指向的页(4KB)写入到磁盘
 * @param fd        文件描述符
 * @param pageNum   页号
 * @param source    缓冲区中页的地址,也就是pPage
 * @return
 */
RC PF_BufferMgr::WritePage(int fd, PageNum pageNum, char *source)
{

#ifdef PF_LOG
    char psMessage[100];
   sprintf (psMessage, "Writing (%d,%d).\n", fd, pageNum);
   WriteLog(psMessage);
#endif

#ifdef PF_STATS
    pStatisticsMgr->Register(PF_WRITEPAGE, STAT_ADDONE);
#endif

    // 检查
    if(source == NULL)
        return PF_NOBUF;

    // 根据页号和页大小计算位置
    long offset = pageNum * (long)pageSize + PF_FILE_HDR_SIZE;
    if (lseek(fd, offset, L_SET) < 0)
        return (PF_UNIX);

    // 写入数据
    int numBytes = write(fd, source, pageSize);
    if (numBytes < 0)
        return (PF_UNIX);
    else if (numBytes != pageSize)
        return (PF_INCOMPLETEWRITE);
    else
        return OK_RC;
}

/**
 * 初始化一个缓冲区槽
 * 当分配一个新槽时，建议执行该函数初始化
 * @param fd        文件描述符
 * @param pageNum   页号
 * @param slot      要初始化的槽号
 * @return
 */
RC PF_BufferMgr::InitPageDesc(int fd, PageNum pageNum, int slot)
{
    // set the slot to refer to a newly-pinned page
    bufTable[slot].fd       = fd;
    bufTable[slot].pageNum  = pageNum;
    bufTable[slot].bDirty   = FALSE;
    bufTable[slot].pinCount = 1;

    // Return ok
    return OK_RC;
}

//------------------------------------------------------------------------------
// Methods for manipulating raw memory buffers
//------------------------------------------------------------------------------

#define MEMORY_FD -2

//
// GetBlockSize
//
// Return the size of the block that can be allocated.  This is simply
// just the size of the page since a block will take up a page in the
// buffer pool.
//
RC PF_BufferMgr::GetBlockSize(int &length) const
{
    length = pageSize;
    return OK_RC;
}


//
// AllocateBlock
//
// Allocates a page in the buffer pool that is not associated with a
// particular file and returns the pointer to the data area back to the
// user.
//
RC PF_BufferMgr::AllocateBlock(char *&buffer)
{
    RC rc = OK_RC;

    // Get an empty slot from the buffer pool
    int slot;
    if ((rc = InternalAlloc(slot)) != OK_RC)
        return rc;

    // Create artificial page number (just needs to be unique for hash table)
    long temp = (long)bufTable[slot].pPage;
    PageNum pageNum = PageNum(temp & 0x00000000ffffffff);

    // Insert the page into the hash table, and initialize the page description entry
    if ((rc = hashTable.Insert(MEMORY_FD, pageNum, slot) != OK_RC) ||
        (rc = InitPageDesc(MEMORY_FD, pageNum, slot)) != OK_RC) {
        // Put the slot back on the free list before returning the error
        Unlink(slot);
        InsertFree(slot);
        return rc;
    }

    // Return pointer to buffer
    buffer = bufTable[slot].pPage;

    // Return success code
    return OK_RC;
}

//
// DisposeBlock
//
// 除了unpin页之外，还需要从哈希桶中释放资源
// 并且将这个槽插入到空闲链表
// 即与AllocateBlock的反向操作
//
RC PF_BufferMgr::DisposeBlock(const char* buffer)
{
    RC rc = OK_RC;
    long temp = (long)buffer;
    auto pageNum = PageNum(temp & 0x00000000ffffffff);
    SlotNum slot;
    // 首先unpin这个页
    if((rc = UnpinPage(MEMORY_FD, pageNum)))
        return rc;

    if((rc = hashTable.Find(MEMORY_FD,pageNum,slot)))
        return rc;

    // 将这个页从哈希桶中移除并加入到空闲链
    if ((rc = hashTable.Delete(MEMORY_FD, pageNum)) ||
        (rc = Unlink(slot)) ||
        (rc = InsertFree(slot)))
        return rc;
    return rc;
}
