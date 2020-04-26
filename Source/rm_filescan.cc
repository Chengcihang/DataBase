//  rm_filescan.cc
//  DataBaseSystem
//  查询器类的实现
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//
#include <unistd.h>
#include <sys/types.h>
#include "../Header/pf.h"
#include "../Header/rm_filehandle.h"
#include "../Header/rm_rid.h"
#include <stdlib.h>
#include <cstring>


RM_FileScan::RM_FileScan(){
    openScan = FALSE;               // openScan时才是true
    this->fileHandle = NULL;        // 查询器要查询的对象文件
    this->currentPH = NULL;         // 当前正在查询的页
    this->scanPage = PF_INVALIDPAGE;// 当前查询的页号，也可由currentPH得到
    this->scanSlot = BEGIN_SCAN;    // 当前查询的记录所在的槽号
    this->attrLen = 0;
    this->attrOffset = -1;
    this->value = NULL;
    scanEnded = true;
}

/**
 * 析构函数
 * 将scanPage unpin
 * 释放value的空间
 */
RM_FileScan::~RM_FileScan(){
//  if(scanEnded == false && hasPagePinned == true && openScan == true){
//    fileHandle->pfh.UnpinPage(scanPage);
//  }
//  if (initializedValue == true){ // free any memory not freed
//    free(value);
//    initializedValue = false;
//  }
    if(openScan)
        fileHandle->pfh->UnpinPage(scanPage);
    if(value){
        free(value);
        value = NULL;
    }
}

/**
 * 一系列比较函数 ,返回 value1 COMP value2
 * @param value1 比较操作符前的数
 * @param value2 比较操作符后的数
 * @param attrtype 比较的属性类型
 * @param attrLength value1/2的字节数
 * @return 比较结果
 */

// 等于关系，相等返回TRUE,不相等返回FALSE
Boolean equal(void * value1, void * value2, AttrType attrtype, int attrLen){
    switch (attrtype) {
        // 直接return,所以不需要break
        case FLOAT: return *(float *)value1 == *(float *)value2;
        case INT:   return *(int *)value1 == *(int *)value2;
        case CHAR:  return (strncmp((char *) value1, (char *) value2, attrLen) == 0);
        case DATE: return *(long *)value1 == *(long *)value2;
        default:
            return FALSE;  // 类型错误时默认返回不等
    }
}

// 小于关系，value1 < value2, 返回TRUE,否则返回FALSE
Boolean less_than(void * value1, void * value2, AttrType attrtype, int attrLen){
    switch (attrtype) {
        case FLOAT: return *(float *)value1 < *(float *)value2;
        case INT:   return *(int *)value1 < *(int *)value2;
        case CHAR:  return (strncmp((char *) value1, (char *) value2, attrLen) < 0);
        case DATE:  return *(long *)value1 < *(long *)value2;
        default:
            return FALSE;  // 类型错误时默认FALSE
    }
}

// 大于关系，value1 > value2, 返回TRUE,否则返回FALSE
Boolean greater_than(void * value1, void * value2, AttrType attrtype, int attrLen){
    switch (attrtype) {
        case FLOAT: return *(float *)value1 > *(float *)value2;
        case INT:   return *(int *)value1 > *(int *)value2;
        case CHAR:  return (strncmp((char *) value1, (char *) value2, attrLen) > 0);
        case DATE:  return *(long *)value1 > *(long *)value2;
        default:
            return FALSE;  // 类型错误时默认返回FALSE
    }
}

// 小于等于关系，value1 <= value2, 返回TRUE,否则返回FALSE
Boolean less_than_or_eq_to(void * value1, void * value2, AttrType attrtype, int attrLen){
    switch (attrtype) {
        case FLOAT: return *(float *)value1 <= *(float *)value2;
        case INT:   return *(int *)value1 <= *(int *)value2;
        case CHAR:  return (strncmp((char *) value1, (char *) value2, attrLen) <= 0);
        case DATE:  return *(long *)value1 <= *(long *)value2;
        default:
            return FALSE;  // 类型错误时默认返回FALSE
    }
}

// 小于等于关系，value1 >= value2, 返回TRUE,否则返回FALSE
Boolean greater_than_or_eq_to(void * value1, void * value2, AttrType attrtype, int attrLen){
    switch (attrtype) {
        case FLOAT: return *(float *)value1 >= *(float *)value2;
        case INT:   return *(int *)value1 >= *(int *)value2;
        case CHAR:  return (strncmp((char *) value1, (char *) value2, attrLen) >= 0);
        case DATE:  return *(long *)value1 >= *(long *)value2;
        default:
            return FALSE;  // 类型错误时默认返回FALSE
    }
}

// 不等于关系，value1 ！= value2, 返回TRUE,否则返回FALSE
Boolean not_equal(void * value1, void * value2, AttrType attrtype, int attrLen){
    switch (attrtype) {
        case FLOAT: return *(float *)value1 != *(float *)value2;
        case INT:   return *(int *)value1 != *(int *)value2;
        case CHAR:  return (strncmp((char *) value1, (char *) value2, attrLen) != 0);
        case DATE:  return *(long *)value1 != *(long *)value2;
        default:
            return FALSE;  // 类型错误时默认返回FALSE
    }
}

/**
 * 配置查询的条件，主要是保存查询的条件信息以及决定比较函数
 * @param fileHandle 查询的数据表
 * @param attrType   比较的属性类型
 * @param attrLen    属性值字节数
 * @param attrOffset 属性值在记录中的偏移
 * @param compOp     比较操作符，用来决定比较函数
 * @param value      比较条件值，记录的中的属性值根据比较函数与之发生关系
 * @param pinHint    是否有客户端ping
 * @return
 */
RC RM_FileScan::OpenScan (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLen,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint) {
    // 查询是否没有关闭
    if (openScan)
        return RM_INVALIDSCAN;
    openScan = TRUE;

    // 检查目标数据表是否有误
    if(fileHandle.isValidFileHeader())
        this->fileHandle = const_cast<RM_FileHandle*>(&fileHandle);
    else
        return (RM_INVALIDFILE);

    // 配置比较函数
    switch(compOp){
        case EQ_OP : comparator = &equal; break;
        case LT_OP : comparator = &less_than; break;
        case GT_OP : comparator = &greater_than; break;
        case LE_OP : comparator = &less_than_or_eq_to; break;
        case GE_OP : comparator = &greater_than_or_eq_to; break;
        case NE_OP : comparator = &not_equal; break;
        case NO_OP : comparator = NULL; break;
        default: return (RM_INVALIDSCAN);
    }

    // 配置其他信息
    int recSize = (this->fileHandle)->GetRecordSize();
    this->compOp = compOp;

    // 配置属性相关项
    if(compOp != NO_OP){ // 需要用到属性比较
        if((attrOffset + attrLen) > recSize || attrOffset < 0 || attrOffset > MAXCHARLEN)
            return (RM_INVALIDSCAN);
        this->attrOffset = attrOffset;
        this->attrLen = attrLen;

        // FLAOT和INT占4字节
        if(attrType == FLOAT || attrType == INT){
            if(attrLen != 4)
                return (RM_INVALIDSCAN);

            this->value = (void *) malloc(4);
            memcpy(this->value, value, 4);
        } else if(attrType == DATE){
            // DATE占8字节
            this->value = (void *) malloc(8);
            memcpy(this->value, value, 8);

        }else if(attrType == CHAR){
            // CHAR类型占attrLen字节
            this->value = (void *) malloc(attrLen);
            memcpy(this->value, value, attrLen);
        }else{
            return (RM_INVALIDSCAN);
        }
        this->attrType = attrType;
    }
    scanEnded = FALSE;     // 可以继续查询
    // set up scan parameters:
//    numRecOnPage = 0;
//    numSeenOnPage = 0;
//    useNextPage = true;
    scanPage = 0;           // 从0号页开始查询
    scanSlot = BEGIN_SCAN;  // 槽号为-1开始
//    numSeenOnPage = 0;
//    hasPagePinned = false;
    return OK_RC;
} 

/**
 * 返回page这个页中有效记录的数量
 * @param page
 * @return 页中的记录数
 */
int RM_FileScan::GetNumRecOnPage(PF_PageHandle &page){
    // 得到页表头
    RM_PageHeader *pageheader = this->fileHandle->GetPageHeader(page);
    return pageheader->numRecords;
}

/**
 * 这个方法只在所给的page页查找下一条记录并返回，不对上层的scan的信息做修改，也不unpin页
 * 也就是只在所给的page页查找scanSlot的下一条记录并返回
 * @param page 查询的页,page必须是这个数据表的某一个页，但是这里无法做检查
 * @param rec  返回slotScan的下一条记录
 * @return OK_RC          得到一条记录
 *         RM_ENDOFPAGE   到达当前文件尾
 */
RC RM_FileScan::GetNextRecord(PF_PageHandle &page, RM_Record &nextRec) {
    RC rc = 0;
    // 解析page的data,得到所需要的部件
    // 页头，记录了该页内的有效记录数和下一个空闲页的页号
    RM_PageHeader *pageHeader = this->fileHandle->GetPageHeader(page);
    // 构造位图对象
    RM_BitMap bitMap = RM_BitMap(this->fileHandle->GetPageBitMap(page),
            this->fileHandle->tableHeader.numRecordsPerPage);

    // 根据slotScan，查找下一个有效的槽
    SlotNum nextSlot = bitMap.GetNextOneBit(this->scanSlot);

    if(nextSlot < 0){
        // 该页无法再找到下一个1的槽位，返回到达文件尾
        return RM_ENDOFPAGE;
    }else{
        // 找到可用的槽位，构建nextRec并返回
        RID rid(page.GetPageNum(),nextSlot);
        nextRec.SetRecord(rid,
                this->fileHandle->GetRecord(page,nextSlot),
                this->fileHandle->GetRecordSize());
        if((rc = nextRec.SetRecord(rid,
                                   this->fileHandle->GetRecord(page,nextSlot),
                                   this->fileHandle->GetRecordSize())))
            return rc;
        return OK_RC;
    }
}

/**
 * 查询执行，每次去得一条记录，返回记录对象RM_Record
 * 1.检查，查询器是否已经创建，是否已经查询完了
 * 2.首先得到当前页作为开始
 * 3.循环
 * 4.  调用GetNextRecord，在当前页得到下一条记录
 *          -如果返回RM_ENDOFPAGE，表示该页搜索完了，没有找到下一条记录，将该页unpin，并
 *           调用GetNextPage，取得下一页
 *              -若不能取得下一页，表示数据表检索完了，返回RM_EOF
 *              -若可以取得下一页，则需要重置scanPage和SacnSlot
 *          -如果返回值不是RM_ENDOFPAGE,表示成功得到一个记录
 *           重置scanPage和scanSlot为这条记录
 *           判断这条记录是否符合查询条件
 *              -符合，返回这条记录，并退出循环
 *              -不符合，啥都不做，继续循环
 * 5. 我这个代码写得真好！！！
 * @param rec
 * @return
 */
RC RM_FileScan::GetNextRec(RM_Record &rec) {
    // 检查
    if(scanEnded == TRUE)
        return RM_EOF;
    if(openScan == FALSE)
        return RM_INVALIDSCAN;

    RC rc;
    PF_PageHandle page;     // 当前查询的页
    // 首先得到要查询的页
    if((rc = this->fileHandle->pfh->GetThisPage(scanPage,page)))
        return rc;
    RM_Record nextRec;
    while (TRUE){
        // 调用GetNextRecord方法，得到下一条记录
        if((rc = this->GetNextRecord(page,nextRec)) == RM_ENDOFPAGE){
            // 当前页搜索完并没有找到下一条记录
            // unpin这个页，并将scanPage指向下一个页
            if((rc = fileHandle->pfh->UnpinPage(scanPage)))
                return rc;
            // scanPage的下一页
            if((rc = fileHandle->pfh->GetNextPage(scanPage,page)) == PF_EOF){
                // 到达文件尾，该数据表检索完全
                scanEnded = TRUE;
                return RM_EOF;
            }
            scanPage = page.GetPageNum(); // 重置scanPage
            scanSlot = BEGIN_SCAN;        // 重置scanSlot
        } else{
            // 找到下一条记录,更新scanPage和scanSlot
            RID rid;
            nextRec.GetRid(rid);
            scanPage = rid.GetPageNum();
            scanSlot = rid.GetSlotNum();
            // 判断这条记录是否符合查询条件，符合就返回这个记录并退出循环,否则继续循环
            // 判断这条记录是否满足条件
            char* pData; // 记录数据
            nextRec.GetData(pData);
            if(compOp != NO_OP){
                bool satisfies = (* comparator)(pData + attrOffset, this->value, attrType, attrLen);
                if(satisfies){
                    rec = nextRec;
                    break;
                }
            }
            else{
                rec = nextRec;
                break;
            }
        }
        // 不满足条件继续查找下一条
    }
    return OK_RC;
}

/**
 * 关闭一个查询器
 * 1.unpin scanPage
 * 2.释放value的资源
 * 3.openScan设置为FALSE
 * @return
 */
RC RM_FileScan::CloseScan () {
    RC rc;
    // 没打开的不能关闭
    if(openScan == FALSE){
        return RM_INVALIDSCAN;
    }
    if((rc = fileHandle->pfh->UnpinPage(scanPage)))
      return (rc);
    if(this->value){
        free(this->value);
        this->value = FALSE;
     }
    openScan = FALSE;
  return OK_RC;
}