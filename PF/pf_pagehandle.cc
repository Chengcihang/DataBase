// File:        pf_pagehandle.cc
// Description: PF_PageHandle class implementation


#include "pf_internal.h"
#include "pf.h"


//
// Defines
//
#define INVALID_PAGE   (-1)

//
// PF_PageHandle
//
// Desc: Default constructor for a page handle object
//       A page handle object provides access to the contents of a page
//       and the page's page number.  The page handle object is constructed
//       here but it must be passed to one of the PF_FileHandle methods to
//       have it refer to a pinned page before it can be used to access the
//       contents of a page.  Remember to call PF_FileHandle::UnpinPage()
//       to unpin the page when you are finished accessing it.
//
PF_PageHandle::PF_PageHandle()
{
    pageNum = INVALID_PAGE;
    pPage = NULL;
}

//
// ~PF_PageHandle
//
// Desc: Destroy the page handle object.
//       If the page handle object refers to a pinned page, the page will
//       NOT be unpinned.
//
PF_PageHandle::~PF_PageHandle()
{
    // Don't need to do anything
}

//
// PF_PageHandle
//
// Desc: Copy constructor
//       If the incoming page handle object refers to a pinned page,
//       the page will NOT be pinned again.
// In:   pageHandle - page handle object from which to construct this object
//
PF_PageHandle::PF_PageHandle(const PF_PageHandle &pageHandle)
{
    // Just copy the local variables since there is no local memory
    // allocation involved
    this->pageNum = pageHandle.pageNum;
    this->pPage = pageHandle.pPage;
}

//
// operator=
//
// Desc: overload = operator
//       If the page handle object on the rhs refers to a pinned page,
//       the page will NOT be pinned again.
// In:   pageHandle - page handle object to set this object equal to
// Ret:  reference to *this
//
PF_PageHandle& PF_PageHandle::operator= (const PF_PageHandle &pageHandle)
{
    // Check for self-assignment
    if (this != &pageHandle) {

        // Just copy the pointers since there is no local memory
        // allocation involved
        this->pageNum = pageHandle.pageNum;
        this->pPage = pageHandle.pPage;
    }

    // Return a reference to this
    return (*this);
}

/**
 * 得到页的数据的首地址
 * 数据在pPage中偏移sizeof(PF_PageHdr)的位置
 * @param pData
 * @return
 */
RC PF_PageHandle::GetPageData(char *&pData) const
{
    // 检查pDage非空
    if (pPage == NULL)
        return (PF_PAGEUNPINNED);

    if (pageNum == PF_FILE_HDR_PAGENUM)     // 文件头页
        pData = pPage + sizeof(PF_FileHdr);

    else    // 设置pData为页数据的地址(在页头之后)
        pData = pPage + sizeof(PF_PageHdr); // 文件内容页

    // Return OK_RC
    return OK_RC;
}

PageNum PF_PageHandle::GetPageNum() const {
    return this->pageNum;
}

RC PF_PageHandle::SetPageNum(PageNum pageNum) {
    this->pageNum = pageNum;
    return OK_RC;
}

/**
 * 得到页头
 * 就是pPage的首个PF_PageHdr字节的数据
 * @param pageHdr
 * @return PF_PAGEFREE pPage为空
 *         OK_RC       成功
 */
RC PF_PageHandle::GetPageHdr(PF_PageHdr &pageHdr) const {
    // 该方法是内容页的文件头方法
    if(pageNum == PF_FILE_HDR_PAGENUM)
        return PF_PAGEFREE;     // 这里随便返回一个错误
    // 检查pPage非空
    if(pPage == NULL)
        return PF_PAGEFREE;
    // 取pPage前PF_PageHdr字节
    pageHdr = *(PF_PageHdr*)this->pPage;

    return OK_RC;
}

/**
 * 页头信息需要修改时修改
 * @param pageHdr 新的页头
 * @return PF_PAGEFREE pPage为空
 *         OK_RC       成功
 */
RC PF_PageHandle::SetPageHdr(PF_PageHdr pageHdr) const {
    // 这是文件内容页的set方法
    if(pageNum == PF_FILE_HDR_PAGENUM)
        return PF_PAGEFREE;
    // 检查pPage非空
    if(pPage == NULL)
        return PF_PAGEFREE;
    // 取pPage前PF_PageHdr字节

    // 将pageHdr复制到缓冲区中的对应位置
    memcpy(this->pPage,&pageHdr,sizeof(PF_PageHdr));

    return OK_RC;
}

/**
 * 构造函数，在栈中构造一个PF_Handle对象
 * 来关联内存中的页
 * @param pageNum
 * @param pPage
 */
PF_PageHandle::PF_PageHandle(PageNum pageNum, char *pPage) {
    this->pageNum = pageNum;
    this->pPage = pPage;
}

/**
 * 设置页的在内存中的地址值
 * @param pPage
 * @return
 */
RC PF_PageHandle::SetPageData(char *pPage) {
    this->pPage = pPage;
    return OK_RC;
}

RC PF_PageHandle::SetPageHdr(PF_FileHdr pageHdr) const {
    // 这是文件头页的set方法
    if(pageNum != PF_FILE_HDR_PAGENUM)
        return PF_PAGEFREE;
    // 检查pPage非空
    if(pPage == NULL)
        return PF_PAGEFREE;
    // 取pPage前PF_PageHdr字节

    // 将pageHdr复制到缓冲区中的对应位置
    memcpy(this->pPage,&pageHdr,sizeof(PF_FileHdr));
    return OK_RC;
}

RC PF_PageHandle::GetPageHdr(PF_FileHdr &pageHdr) const {
    // 该方法是文件头页的文件头方法
    if(pageNum != PF_FILE_HDR_PAGENUM)
        return PF_PAGEFREE;     // 这里随便返回一个错误
    // 检查pPage非空
    if(pPage == NULL)
        return PF_PAGEFREE;
    // 取pPage前PF_PageHdr字节
    pageHdr = *(PF_FileHdr*)this->pPage;
    return OK_RC;
}


