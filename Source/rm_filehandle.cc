//  rm_filehandle.cc
//  DataBaseSystem
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//  不足，当其中某个过程中的错误被触发时，可能不会执行后续的必要操作
//   甚至之前的某个操作也需要回滚，然而不知道编程该如何实现
//

#include <unistd.h>
#include <sys/types.h>
#include "../Header/rm_filehandle.h"
#include "../Header/rm_bitmap.h"
#include <math.h>
#include <cstdio>
#include <cstring>

/**
 * @brief 构造函数
 * 其实没用
 * @param 无
 * @return 无
 */
RM_FileHandle::RM_FileHandle(){
    this->ifHeaderModified = FALSE;
    this->pfh = NULL;
    this->isOpened = FALSE;
}

/**
 * @brief 析构函数
 * 其实没用
 * @param 无
 * @return 无
 */
RM_FileHandle::~RM_FileHandle(){
    this->isOpened = FALSE;
}

/**
 * @brief =操作符重载
 * @param RM_FileHandle &fileHandle
 * @return (*this)
 */
RM_FileHandle& RM_FileHandle::operator= (const RM_FileHandle &fileHandle){
    if (this != &fileHandle){
        // 复制所有变量
        this->isOpened = fileHandle.isOpened;
        this->ifHeaderModified = fileHandle.ifHeaderModified;
        this->pfh = fileHandle.pfh;
        memcpy(&this->tableHeader, &fileHandle.tableHeader, sizeof(RM_FileHeader));
  }
  return (*this);
}

/**
 * @brief 从页pageHandle中取得有关记录的页头，
 *        该信息存储在pageHandle.pPage+sizeof(PF_PageHdr)的位置
 * @param pageHandle
 * @return 记录页头的地址
 */
RM_PageHeader * RM_FileHandle::GetPageHeader(const PF_PageHandle &pageHandle) const {
    // 返回页的数据域的首地址即可
    char * pdata;
    RC rc;
    pageHandle.GetPageData(pdata);
    return (RM_PageHeader *)pdata;
}

/**
 * 得到位图的首地址
 * @param pageHandle
 * @return
 */
char * RM_FileHandle::GetPageBitMap(const PF_PageHandle &pageHandle) const {
    // 返回页的数据域之后的
    return (char *)GetPageHeader(pageHandle) + sizeof(RM_PageHeader);
}

/**
 * 得到槽号slot的记录数据地址
 * 位图地址+位图占用字节数+slot*一条记录长度
 * @param slot
 * @return
 */
char *RM_FileHandle::GetRecord(const PF_PageHandle &pageHandle, SlotNum slot) const {
    return (char *)GetPageBitMap(pageHandle)
    + this->tableHeader.bitmapLen
    + slot * this->tableHeader.recordSize;
}

/**
 * 分配一个新页，调用pfh的分配新页的方法，同时修改数据表头的信息
 * 也就是numPages(数据表占用的页数),firstFreePage(有剩余空间页的首页号)
 * @param newPage 这个新页的句柄
 * @return 错误信息
 */
RC RM_FileHandle::AllocateNewPage(PF_PageHandle &newPage) {
    RC rc;
    // 调用pfh的AllocatePage方法得到一个新页
    if((rc = pfh->AllocatePage(newPage))){
        return (rc);
    }
    // 初始化这个新页，补充RM_PageHeader
    RM_PageHeader *pageHeader = GetPageHeader(newPage);
    char * bitmap = GetPageBitMap(newPage);
    pageHeader->nextFreePage = tableHeader.firstFreePage;
    pageHeader->numRecords = 0;

    // 初始化位图
    RM_BitMap bitMap = RM_BitMap(bitmap,this->tableHeader.numRecordsPerPage);
    if((rc = bitMap.Reset()))
        return rc;

    // 修改数据表头信息
    // firstFreePage为这个新页的页号
    tableHeader.firstFreePage = newPage.GetPageNum();
    // numPages增加
    tableHeader.numPages++;

    ifHeaderModified = TRUE;

    return OK_RC;
}


/**
 * @brief =根据rid，从表中得到一条记录，生成RM_Record返回
 * 1.检查：rid是否有效以及表是否为打开状态，无效返回错误信息
 * 2.解析rid,得到目标记录的页号和槽号，调用pfh的getthispage
 *   方法得到这个页。
 * 3.检查位图，该槽号表示的记录是否存在，若不存在返回错误信息，
 *   否在，根据槽号定位数据并返回
 * 4.退出之间unpin这个页
 * @param const RID &rid 记录的rid
 * @param RM_Record &rec 返回的记录实例
 * @return 错误信息
 *          OK_RC               无任何错误
 *          RM_INVALIDFILE      无效的文件
 *          RM_INVALIDRECORD    记录不存在
 *          RM_INVALIDRID       无效的rid
 */
RC RM_FileHandle::GetRec (const RID &rid, RM_Record &rec) const {
    // 1.检查
    if(!rid.isValidRID())
        return RM_INVALIDRID;
    if(!this->isOpened)
        return RM_INVALIDFILE;

    // 解析rid
    PageNum pageNum = rid.GetPageNum();
    SlotNum slotNum = rid.GetSlotNum();

    // 页
    RC rc;
    PF_PageHandle page;
    // 取出这个页，若出现问题则返回错误信息
    if((rc = pfh->GetThisPage(pageNum,page)))
        return rc;

    // 取得位图的数据地址，并构造位图对象，方便使用
    RM_BitMap bitMap = RM_BitMap(GetPageBitMap(page),
            this->tableHeader.numRecordsPerPage);

    // 检查slotNum的数据是否存在
    if(!bitMap.CheckBit(slotNum)){
        // 该位置的记录不存在,unpin这个页，返回错误信息
        if((rc = pfh->UnpinPage(pageNum)))
            return (rc);
        return RM_INVALIDRECORD;
    }else{
        // 这个记录存在，取出这个记录的数据，修改rec,unpin这个页并返回错误信息
        rc = rec.SetRecord(rid, GetRecord(page,slotNum),
                               this->tableHeader.recordSize);
        RC rc2;
        if((rc2 = pfh->UnpinPage(pageNum)))
            return rc2;
        return rc;
    }
}

/**
 * 将pData中的数据插入到页数据表中，返回插入记录的rid.
 * 1.检查：文件打开，pData不为NULL
 * 2.检查tableHeader.firstFreePage是否等于NO_MORE_FREE_PAGES
 *      == NO_MORE_FREE_PAGES, 分配一个新页来插入记录
 *     ！= NO_MORE_FREE_PAGES，取得这个页插入数据
 * @param pData
 * @param rid
 * @return
 */
RC RM_FileHandle::InsertRec (const char *pData, RID &rid) {
    if(!this->isOpened)
        return RM_INVALIDFILE;
    if(pData == NULL)
        return RM_INVALIDRECORD;

    RC rc = OK_RC;
    PF_PageHandle page; // 申明一个页
    if(tableHeader.firstFreePage == NO_MORE_FREE_PAGES){
        // 没有可用的空闲页,申请一个新页
        if((rc = AllocateNewPage(page)))
            return rc;

        ifHeaderModified = TRUE;
    }else{
        // 取得这个空闲页
        if((rc = pfh->GetThisPage(tableHeader.firstFreePage,page)))
            return rc;
    }

    // 取得这个空闲页的位图，并查找可用的槽位
    char * bitmap = GetPageBitMap(page);
    RM_BitMap bitMap = RM_BitMap(bitmap,tableHeader.numRecordsPerPage); // 每页的记录数目等价于位图的位数
    SlotNum slot = bitMap.GetFirstZeroBit();

    // 插入数据
    memcpy(bitMap.GetBitMap() + (tableHeader.bitmapLen) + slot * (tableHeader.recordSize),
           pData, tableHeader.recordSize);
    // 记录数加一
    RM_PageHeader *pageHeader = GetPageHeader(page);
    pageHeader->numRecords++;
    // 修改位图
    if((rc = bitMap.SetBit(slot)))
        return rc;
    // 标记脏页并unpin
    if((rc = pfh->MarkDirty(page.GetPageNum()) ) || (rc = pfh->UnpinPage(page.GetPageNum())))
        return rc;
    // 返回rid
    rid = RID(page.GetPageNum(),slot);
    return OK_RC;
}


/**
 * 根据rid删除记录，就是将位图的对应设置为0
 * 1.检查：文件是否打开，传入参数rid是否合法
 * 2.根据rid的pageNum取得该页
 * 3.根据rid的slotNum将位图的对应位设置为0
 * 4.结束前标记为脏页并且unpin
 * @param rid
 * @return
 */
RC RM_FileHandle::DeleteRec (const RID &rid) {
    // 检查
    if(!this->isOpened)
        return RM_INVALIDFILE;
    if(!rid.isValidRID())
        return RM_INVALIDRID;
    // 取得页号和槽号
    PageNum pageNum = rid.GetPageNum();
    SlotNum slotNum = rid.GetSlotNum();

    //取得页
    RC rc = OK_RC;
    PF_PageHandle page;
    if((rc = pfh->GetThisPage(pageNum,page)))
        return rc;

    // 取得位图
    char *bitmap = GetPageBitMap(page);
    RM_BitMap bitMap = RM_BitMap(bitmap,tableHeader.numRecordsPerPage);

    // 判断该位的记录是否存在
    if(!bitMap.CheckBit(slotNum))
        return RM_INVALIDRECORD;

    // 对应位设置0,记录数减1
    bitMap.ResetBit(slotNum);

    RM_PageHeader *pageHeader = GetPageHeader(page);
    pageHeader->numRecords--;

    // 如果该页从满页变成了空闲页，修改数据表头和页头nextFree字段
    if(pageHeader->numRecords == tableHeader.numRecordsPerPage-1){
        pageHeader->nextFreePage = tableHeader.firstFreePage;
        tableHeader.firstFreePage = page.GetPageNum();
        ifHeaderModified = TRUE;
    }

    // 标记脏页并unpin
    if((rc = pfh->MarkDirty(page.GetPageNum()) ) || (rc = pfh->UnpinPage(page.GetPageNum())))
        return rc;

    return rc;
}

/**
 * rec中有这条记录的rid,根据rid可以找到这条记录,再利用rec中的pdata更新之
 * 1.检查:文件打开，传入参数是否有效
 * 2.调用GetRec方法得到数据中的原始记录oldrec
 * 3.利用rec的pdata覆盖掉oldrec的pdata
 * 4.标记脏页和unpin
 * @param rec
 * @return
 */
RC RM_FileHandle::UpdateRec (const RM_Record &newRec) {
    //检查
    if(!isOpened)
        return RM_INVALIDFILE;
    RID rid;
    newRec.GetRid(rid);
    if(!rid.isValidRID())
        return RM_INVALIDRID;

    // 获取旧记录
    RM_Record oldRec;
    GetRec(rid,oldRec);

    char *oldData,*newData;
    oldRec.GetData(oldData);
    newRec.GetData(newData);

    // 新记录覆盖旧记录
    memcpy(oldData, newData, tableHeader.recordSize);

    //标记脏页并unpin
    RC rc = OK_RC;
    if((rc = pfh->MarkDirty(rid.GetPageNum())) || (rc = pfh->UnpinPage(rid.GetPageNum())))
        return rc;
    return rc;
}


/**
 * 将pageNum的页回写磁盘，调用pfh的ForcePages方法
 * @param pageNum 页号
 * @return
 */
RC RM_FileHandle::ForcePages(PageNum pageNum) {
    if(!isOpened)
        return RM_INVALIDFILE;
    pfh->ForcePages(pageNum);
    return OK_RC;
}


/**
 * 判断数据表头是否有效
 * @return
 */
Boolean RM_FileHandle::isValidFileHeader() const{
    if(!isOpened)
        return FALSE;

    if(tableHeader.recordSize <= 0 || tableHeader.numRecordsPerPage <=0 || tableHeader.numPages <= 0)
        return FALSE;

    if((sizeof(RM_PageHeader) + tableHeader.bitmapLen + tableHeader.recordSize*tableHeader.numRecordsPerPage) >
    PF_PAGE_SIZE){
        return FALSE;
    }
    return TRUE;
}

// 返回记录的字节数
// 可有可无
int RM_FileHandle::GetRecordSize() const {
    return this->tableHeader.recordSize;
}


