// File:        pf_filehandle.cc
// Description: PF_FileHandle class implementation


#include <unistd.h>
#include <sys/types.h>
#include "../Header/pf_internal.h"
#include "../Header/pf_buffermgr.h"
#include "../Header/pf.h"


//
// PF_FileHandle
//
// Desc: Default constructor for a file handle object
//       A File object provides access to an open file.
//       It is used to allocate, dispose and fetch pages.
//       It is constructed here but must be passed to PF_Manager::OpenFile() in
//       order to be used to access the pages of a file.
//       It should be passed to PF_Manager::CloseFile() to close the file.
//       A file handle object contains a pointer to the file data stored
//       in the file table managed by PF_Manager.  It passes the file's unix
//       file descriptor to the buffer manager to access pages of the file.
//
PF_FileHandle::PF_FileHandle()
{
    // Initialize local variables
    bFileOpen = FALSE;
    pBufferMgr = NULL;
}

//
// ~PF_FileHandle
//
// Desc: Destroy the file handle object
//       If the file handle object refers to an open file, the file will
//       NOT be closed.
//
PF_FileHandle::~PF_FileHandle()
{
    // Don't need to do anything
}

//
// PF_FileHandle
//
// Desc: copy constructor
// In:   fileHandle - file handle object from which to construct this object
//
PF_FileHandle::PF_FileHandle(const PF_FileHandle &fileHandle)
{
    // Just copy the data members since there is no memory allocation involved
    this->pBufferMgr  = fileHandle.pBufferMgr;
    this->hdr         = fileHandle.hdr;
    this->bFileOpen   = fileHandle.bFileOpen;
    this->bHdrChanged = fileHandle.bHdrChanged;
    this->unixfd      = fileHandle.unixfd;
}

//
// operator=
//
// Desc: overload = operator
//       If this file handle object refers to an open file, the file will
//       NOT be closed.
// In:   fileHandle - file handle object to set this object equal to
// Ret:  reference to *this
//
PF_FileHandle& PF_FileHandle::operator= (const PF_FileHandle &fileHandle)
{
    // Test for self-assignment
    if (this != &fileHandle) {

        // Just copy the members since there is no memory allocation involved
        this->pBufferMgr  = fileHandle.pBufferMgr;
        this->hdr         = fileHandle.hdr;
        this->bFileOpen   = fileHandle.bFileOpen;
        this->bHdrChanged = fileHandle.bHdrChanged;
        this->unixfd      = fileHandle.unixfd;
    }

    // Return a reference to this
    return (*this);
}


/**
 * 得到当前页的下一个页，中间有些页可能无效
 * 这个方法是返回current之后可用的页
 * @param current       当前页号
 * @param pageHandle    下一个页对象
 * @return
 */
RC PF_FileHandle::GetNextPage(PageNum current, PF_PageHandle &pageHandle) const
{
    int rc;
    // 不做检查也可以，因为GetThisPage会做同样的检查
    if (!bFileOpen)
        return (PF_CLOSEDFILE);

    // 参数有效性检查
    if (!IsValidPageNum(current))
        return (PF_INVALIDPAGE);
    // 从current+1开始遍历，找到一个有效页就返回
    for (int i = current+1; i < hdr->numPages; i++) {

        if (!(rc = GetThisPage(i, pageHandle)))
            return OK_RC;

        if (rc != PF_INVALIDPAGE)
            return (rc);
    }

    // 没有下一页
    return (PF_EOF);
}

/**
 * 得到当前页的上一个可用页，这个方法是返回current之前可用的页
 * @param current
 * @param pageHandle 上一个页对象
 * @return
 */
RC PF_FileHandle::GetPrevPage(PageNum current, PF_PageHandle &pageHandle) const
{
    int rc;

    if (!bFileOpen)
        return (PF_CLOSEDFILE);

    if (current != hdr->numPages &&  !IsValidPageNum(current))
        return (PF_INVALIDPAGE);

    for (PageNum i = current-1; i >= 0; i--) {
        if (!(rc = GetThisPage(i, pageHandle)))
            return OK_RC;
        if (rc != PF_INVALIDPAGE)
            return (rc);
    }
    return (PF_EOF);
}


/**
 * 得到这个文件的第pageNum的页
 * 调用缓冲去的GetPage方法，得到这个页
 * @param pageNum       页号
 * @param pageHandle    页实例
 * @return
 */
RC PF_FileHandle::GetThisPage(PageNum pageNum, PF_PageHandle &pageHandle) const
{
    int  rc;
    char *pPageBuf;

    // 检查文件是否打开
    if (!bFileOpen)
        return (PF_CLOSEDFILE);

    // 页号是否正确
    if (!IsValidPageNum(pageNum))
        return (PF_INVALIDPAGE);

    // 调用缓冲区的的方法获得这个页
    if ((rc = pBufferMgr->GetPage(unixfd, pageNum, &pPageBuf)))
        return (rc);

    // 该页是有效页 或者是 文件头页
    if (((PF_PageHdr*)pPageBuf)->nextFree == PF_PAGE_USED || pageNum == PF_FILE_HDR_PAGENUM) {
        // 设置页号
        pageHandle.SetPageNum(pageNum);
        // 设置页数据
        pageHandle.SetPageData(pPageBuf);
        // Return ok
        return OK_RC;
    }

    // 如果操作失败需要UnpinPage
    if ((rc = UnpinPage(pageNum)))
        return (rc);

    return (PF_INVALIDPAGE);
}

//
// AllocatePage
//
// Desc: Allocate a new page in the file (may get a page which was
//       previously disposed)
//       The file handle must refer to an open file
// Out:  pageHandle - becomes a handle to the newly-allocated page
//                    this function modifies local var's in pageHandle
// Ret:  PF return code
//
RC PF_FileHandle::AllocatePage(PF_PageHandle &pageHandle)
{
    int     rc;               // return code
    int     pageNum;          // new-page number
    char    *pPageBuf;        // address of page in buffer pool

    // File must be open
    if (!bFileOpen)
        return (PF_CLOSEDFILE);

    // If the free list isn't empty...
    if (hdr->firstFree != PF_PAGE_LIST_END) {
        pageNum = hdr->firstFree;

        // Get the first free page into the buffer
        if ((rc = pBufferMgr->GetPage(unixfd,
                                      pageNum,
                                      &pPageBuf)))
            return (rc);

        // Set the first free page to the next page on the free list
        hdr->firstFree = ((PF_PageHdr*)pPageBuf)->nextFree;
    }
    else {

        // The free list is empty...
        pageNum = hdr->numPages;

        // Allocate a new page in the file
        if ((rc = pBufferMgr->AllocatePage(unixfd,
                                           pageNum,
                                           &pPageBuf)))
            return (rc);

        // Increment the number of pages for this file
        hdr->numPages++;
    }

    // Mark the header as changed
    bHdrChanged = TRUE;

    // Mark this page as used
    ((PF_PageHdr *)pPageBuf)->nextFree = PF_PAGE_USED;

    // Zero out the page data
    memset(pPageBuf + sizeof(PF_PageHdr), 0, PF_PAGE_SIZE);

    // Mark the page dirty because we changed the next pointer
    if ((rc = MarkDirty(pageNum)))
        return (rc);

    // Set the pageHandle local variables
    pageHandle.SetPageNum(pageNum);
    pageHandle.SetPageData(pPageBuf);

    // Return ok
    return OK_RC;
}

//
// DisposePage
//
// Desc: Dispose of a page
//       The file handle must refer to an open file
//       PF_PageHandle objects referring to this page should not be used
//       after making this call.
// In:   pageNum - number of page to dispose
// Ret:  PF return code
//
RC PF_FileHandle::DisposePage(PageNum pageNum)
{
    int     rc;               // return code
    char    *pPageBuf;        // address of page in buffer pool

    // File must be open
    if (!bFileOpen)
        return (PF_CLOSEDFILE);

    // Validate page number
    if (!IsValidPageNum(pageNum))
        return (PF_INVALIDPAGE);

    // Get the page (but don't re-pin it if it's already pinned)
    if ((rc = pBufferMgr->GetPage(unixfd,
                                  pageNum,
                                  &pPageBuf,
                                  FALSE)))
        return (rc);

    // Page must be valid (used)
    if (((PF_PageHdr *)pPageBuf)->nextFree != PF_PAGE_USED) {

        // Unpin the page
        if ((rc = UnpinPage(pageNum)))
            return (rc);

        // Return page already free
        return (PF_PAGEFREE);
    }

    // Put this page onto the free list
    ((PF_PageHdr *)pPageBuf)->nextFree = hdr->firstFree;
    hdr->firstFree = pageNum;
    bHdrChanged = TRUE;

    // Mark the page dirty because we changed the next pointer
    if ((rc = MarkDirty(pageNum)))
        return (rc);

    // Unpin the page
    if ((rc = UnpinPage(pageNum)))
        return (rc);

    // Return ok
    return OK_RC;
}

/**
 * 调用缓冲区的MarkDirty方法
 * @param pageNum
 * @return
 */
RC PF_FileHandle::MarkDirty(PageNum pageNum) const
{
    // 常规检查
    if (!bFileOpen)
        return (PF_CLOSEDFILE);
    if (!IsValidPageNum(pageNum))
        return (PF_INVALIDPAGE);

    // 调用缓冲区的方法
    return (pBufferMgr->MarkDirty(unixfd, pageNum));
}

/**
 * 调用缓冲区的unpin方法
 * @param pageNum
 * @return
 */
RC PF_FileHandle::UnpinPage(PageNum pageNum) const
{
    if (!bFileOpen)
        return (PF_CLOSEDFILE);

    if (!IsValidPageNum(pageNum))
        return (PF_INVALIDPAGE);
    // 调用缓冲区的方法
    return (pBufferMgr->UnpinPage(unixfd, pageNum));
}

/**
 * 将该文件的所有页都回写磁盘，如果头文件页被修改
 * 则会将文件头页回写
 * 使用时请确保所有的页的pinCount都为0,否则报错
 * @return
 */
RC PF_FileHandle::FlushPages() const
{
    // File must be open
    if (!bFileOpen)
        return (PF_CLOSEDFILE);

    // 文件头发生了修改，需要将文件头所在的页fd/PF_FILEHDR_PAGENUM回写
    if (bHdrChanged) {
//        // 文件头的便宜为0
//        if (lseek(unixfd, 0, L_SET) < 0)
//            return (PF_UNIX);
//
//        // 整个文件页都回写
//        int numBytes = write(unixfd,
//                             (char *)hdr,
//                             PF_FILE_HDR_SIZE);
//        if (numBytes < 0)
//            return (PF_UNIX);
//        if (numBytes != PF_FILE_HDR_SIZE)
//            return (PF_HDRWRITE);
        // 将文件头所在的页回写磁盘
        pBufferMgr->ForcePages(unixfd,PF_FILE_HDR_PAGENUM);

        // 标志位重置
        PF_FileHandle *dummy = (PF_FileHandle *)this;
        dummy->bHdrChanged = FALSE;
    }

    // 调用缓冲区的方法将所有页回写
    return (pBufferMgr->FlushPages(unixfd));
}

/**
 *
 * @param pageNum
 * @return
 */
RC PF_FileHandle::ForcePages(PageNum pageNum) const
{
    //检查
    if (!bFileOpen)
        return (PF_CLOSEDFILE);

    //如果文件头修改，需要将文件头页回写磁盘
    if (bHdrChanged) {

        if (lseek(unixfd, 0, L_SET) < 0)
            return (PF_UNIX);

        // 文件头回写磁盘
        int numBytes = write(unixfd,
                             (char *)hdr,
                             PF_FILE_HDR_SIZE);
        if (numBytes < 0)
            return (PF_UNIX);
        if (numBytes != PF_FILE_HDR_SIZE)
            return (PF_HDRWRITE);

        PF_FileHandle *dummy = (PF_FileHandle *)this;
        dummy->bHdrChanged = FALSE;
    }
    // 调用缓冲区的方法
    return (pBufferMgr->ForcePages(unixfd, pageNum));
}

// 页号大于等于-1(文件头的页号)，小于总页数
int PF_FileHandle::IsValidPageNum(PageNum pageNum) const
{
    if(hdr == NULL){
        return bFileOpen && pageNum >= PF_FILE_HDR_PAGENUM;
    } else{
        return (bFileOpen &&
                pageNum >= PF_FILE_HDR_PAGENUM &&
                pageNum < hdr->numPages);
    }
}

/**
 * 当需要对文件头，数据表头进行操作时，需要将文件头页加载进缓冲区
 * @param    hdrPage 栈中对象，其*pPage指向这个缓冲区页头所在槽的*pPage
 * @return
 */
RC PF_FileHandle::GetFileHdrPage(PF_PageHandle &hdrPage) const {
    RC rc = OK_RC;
    // 调用GetThisPage方法直接获得页号为PF_FILE_HDR_PAGENUM的页，这个页就是文件头页
    if((rc = GetThisPage(PF_FILE_HDR_PAGENUM,hdrPage)))
        return rc;
    return OK_RC;
}

RC PF_FileHandle::SetHdrChanged() {
    this->bHdrChanged = TRUE;
    return OK_RC;
}

