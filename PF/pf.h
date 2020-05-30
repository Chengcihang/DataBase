#ifndef PF_H
#define PF_H

#include "../redbase.h"

typedef int PageNum;

typedef int SlotNum;


#define PF_PAGE_LIST_END  -1       // end of list of free pages
#define PF_PAGE_USED      -2       // page is being used

typedef struct PF_PageHdr {
    PageNum nextFree;       // nextFree can be any of these values:
    //  - the number of the next free page
    //  - PF_PAGE_LIST_END if this is last free page
    //  - PF_PAGE_USED if the page is not free
}PF_PageHdr;

// 页中有效数据的空间大小
const int PF_PAGE_SIZE = 4096 - sizeof(PF_PageHdr);

/**
 * 文件头也占用一个页的位置
 * 文件头的数据从页的pageData的位置开始写
 * 而不是从pData的位置开始写，方便管理
 */
typedef struct PF_FileHdr {
    PageNum firstFree;     // first free page in the linked list
    int numPages;          // # of pages in the file
}PF_FileHdr ;

/**
 * 页在内存中由页头和页内数据两部分组成
 * pageNum为页号；*pPage为内存中整个页的首地址
 * 页的类型有两种，文件头页和文件内容页，两者用页号进行区分
 * 文件头页的页号为PF_FILE_HDR_PAGENUM；文件内容页的页号为非负数
 */
class PF_PageHandle {
    //friend class PF_FileHandle;
public:
    PF_PageHandle  ();                            // Default constructor
    PF_PageHandle  (PageNum pageNum, char*pPage);
    ~PF_PageHandle ();                            // Destructor

    // Copy constructor
    PF_PageHandle  (const PF_PageHandle &pageHandle);
    // Overloaded =
    PF_PageHandle& operator=(const PF_PageHandle &pageHandle);

    RC GetPageData     (char *&pData) const;            // 得到该页除了页头信息的数据

    RC GetPageHdr      (PF_PageHdr &pageHdr) const ;    // 得到这个页的页头

    RC SetPageHdr      (PF_PageHdr pageHdr) const ;     // 设置这个页的页头

    RC GetPageHdr      (PF_FileHdr &pageHdr) const ;    // 得到这个文件头页的页头

    RC SetPageHdr      (PF_FileHdr pageHdr) const ;     // 设置这个文件头页的页头

    PageNum GetPageNum () const ;                       // 得到页号

    RC SetPageNum      (PageNum pageNum);               // 设置页号

    RC SetPageData     (char *pPage);                   // 设置数据

private:
    PageNum pageNum;                                    // page number
    char *pPage;                                        // 这个页在缓冲池中的地址
};

/**
 * PF_FileHandle的大部分操作是调用缓冲区管理类的方法
 */
class PF_BufferMgr;

const PageNum PF_FILE_HDR_PAGENUM = -1;
/**
 * PF_FIleHandle的对象创建在栈中
 * 保存了一个文件的状态信息，该文件的相关配置数据
 * 记录页文件头页，页号为PF_FILE_HDR_PAGENUM
 */
class PF_FileHandle {
    friend class PF_Manager;
public:
    PF_FileHandle  ();                            // Default constructor
    ~PF_FileHandle ();                            // Destructor

    // Copy constructor
    PF_FileHandle  (const PF_FileHandle &fileHandle);

    // Overload =
    PF_FileHandle& operator=(const PF_FileHandle &fileHandle);

    // 得到这个文件的第pageNum的页
    RC GetThisPage (PageNum pageNum, PF_PageHandle &pageHandle) const;

    // 得到文件头页
    RC GetFileHdrPage(PF_PageHandle &hdrPage) const;

    // 得到当前页的下一页
    RC GetNextPage (PageNum current, PF_PageHandle &pageHandle) const;

    // 得到当前页的上一页
    RC GetPrevPage (PageNum current, PF_PageHandle &pageHandle) const;

    RC AllocatePage(PF_PageHandle &pageHandle);    // 为文件再分配一个页

    RC DisposePage (PageNum pageNum);              // 回收文件的一个页

    // 标记脏页
    RC MarkDirty   (PageNum pageNum) const;

    // Unpin页
    RC UnpinPage   (PageNum pageNum) const;

    // 将该文件的所有页回写磁盘，包括文件头所在的页
    RC FlushPages  () const;

    // 将pageNum的页回写磁盘，但不需要释放缓冲区
    RC ForcePages  (PageNum pageNum=ALL_PAGES) const;

    // 由于上层的操作导致文件头页被修改时，提供给上层函数调用
    RC SetHdrChanged();


private:
    // 由于提供的接口参数大多为pageNum
    // 因此有必要对页号进行有效性判断
    int IsValidPageNum (PageNum pageNum) const;
    PF_FileHdr *hdr;                               // 缓冲区中文件头的地址
    Boolean bFileOpen;                             // 文件打开标志，当文件头页被加载i到缓冲区中时为TRUE
    Boolean bHdrChanged;                           // 文件头是否被修改标志
    int unixfd;                                    // 文件描述符
    PF_BufferMgr *pBufferMgr;                      // 缓冲区的指针，用来调用缓冲区的方法
};

/**
 * 文件系统管理类
 */
class PF_Manager {
public:
    PF_Manager    ();                              // Constructor
    ~PF_Manager   ();                              // Destructor
    RC CreateFile    (const char *fileName);       // 创建文件
    RC DestroyFile   (const char *fileName);       // 销毁文件

    // 打开文件
    RC OpenFile      (const char *fileName, PF_FileHandle &fileHandle);

    // 关闭文件
    RC CloseFile     (PF_FileHandle &fileHandle);

    // 缓冲区相关函数
    RC ClearBuffer   ();
    RC PrintBuffer   ();
    RC ResizeBuffer  (int iNewSize);

    RC GetBlockSize  (int &length) const;
    // 分配一个块
    RC AllocateBlock (char *&buffer);
    // 销毁一个块
    RC DisposeBlock  (char *buffer);

private:
    PF_BufferMgr *pBufferMgr;                  // 缓冲区指针
};

//
// Print-error function and PF return code defines
//
void PF_PrintError(RC rc);

#define PF_PAGEPINNED      (START_PF_WARN + 0) // page pinned in buffer
#define PF_PAGENOTINBUF    (START_PF_WARN + 1) // page isn't pinned in buffer
#define PF_INVALIDPAGE     (START_PF_WARN + 2) // invalid page number
#define PF_FILEOPEN        (START_PF_WARN + 3) // file is open
#define PF_CLOSEDFILE      (START_PF_WARN + 4) // file is closed
#define PF_PAGEFREE        (START_PF_WARN + 5) // page already free
#define PF_PAGEUNPINNED    (START_PF_WARN + 6) // page already unpinned
#define PF_EOF             (START_PF_WARN + 7) // end of file
#define PF_TOOSMALL        (START_PF_WARN + 8) // Resize buffer too small
#define PF_INVALIDSLOT     (START_PF_WARN + 9) // invalid slot number
#define PF_LASTWARN        PF_TOOSMALL

#define PF_NOMEM           (START_PF_ERR - 0)  // no memory
#define PF_NOBUF           (START_PF_ERR - 1)  // no buffer space
#define PF_INCOMPLETEREAD  (START_PF_ERR - 2)  // incomplete read from file
#define PF_INCOMPLETEWRITE (START_PF_ERR - 3)  // incomplete write to file
#define PF_HDRREAD         (START_PF_ERR - 4)  // incomplete read of header
#define PF_HDRWRITE        (START_PF_ERR - 5)  // incomplete write to header

// Internal errors
#define PF_PAGEINBUF       (START_PF_ERR - 6) // new page already in buffer
#define PF_HASHNOTFOUND    (START_PF_ERR - 7) // hash table entry not found
#define PF_HASHPAGEEXIST   (START_PF_ERR - 8) // page already in hash table
#define PF_INVALIDNAME     (START_PF_ERR - 9) // invalid PC file name

// Error in UNIX system call or library routine
#define PF_UNIX            (START_PF_ERR - 10) // Unix error
#define PF_LASTERROR       PF_UNIX

#endif
