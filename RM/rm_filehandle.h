//  rm_filehandle.h
//  DataBaseSystem
//  filehandle对应的是数据表，提够对数据表基础的增删改查操作
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef DATABASE_RM_FILEHANDLE_H
#define DATABASE_RM_FILEHANDLE_H
#include "../PF/pf.h"
#include "rm.h"
#include "rm_record.h"
#include "rm_bitmap.h"

/**
 * @brief 记录页头——每个页记录区域之前的字段，保存了该页有效的记录数
 *        和下一个有空闲空间页的页号。
 *        如果该页是个满页，则nextFreePage字段为NO_MORE_FREE_PAGES;
 *        第一个空闲空间页的页号由RM_FileHeader中的firstFreePage字段维护
 */
typedef struct rm_PageHeader {
    PageNum nextFreePage;
    int numRecords;
}RM_PageHdr;

#define NO_MORE_FREE_PAGES -1

/**
 * @brief 数据表头——维护了一张数据表的必要信息
 */
typedef struct rm_FileHeader {
    int recordSize;           // 一条记录占用的字节数
    int numRecordsPerPage;    // 每页的可用的记录数,等于位图的有效位数
    int numPages;             // 数据表占用的页数
    PageNum firstFreePage;    // 有剩余空间页的首页号

    // 位图记录了记录在页中的存储情况，1表示有效记录，0表示无效记录
    int bitmapLen;            // 位图占用的字节数
}RM_FileHdr;

/**
 * @brief 数据表类，是表的抽象
 * 数据存储在文件头的4KB中
 * 提供增删改操作
 */
class RM_FileHandle {
    //每页存的记录之前，nextFree字段，指向下一个有空闲槽的页号
    // 若该页已无空闲空间，该字段设置为NO_FREE_SLOT
    static const PageNum NO_FREE_SLOT = -1;
    friend class RM_Manager;
    friend class RM_FileScan;
public:
    RM_FileHandle ();                                              // 构造函数
    ~RM_FileHandle();                                              // 析构函数
    RM_FileHandle& operator= (const RM_FileHandle &fileHandle);    // 复制操作符重载

    // 增、删、改、查
    RC InsertRec  (const char *pData, RID &rid);                   // 插入一条记录

    RC DeleteRec  (const RID &rid);                                // 删除一条记录

    RC UpdateRec  (const RM_Record &newRec);                       // 更新一条记录

    RC GetRec     (const RID &rid, RM_Record &rec) const;          // 根据rid取得记录

    RC ForcePages (PageNum pageNum = ALL_PAGES);                   // 必要时将页号为pageNum的页回写磁盘
private:
    // 获得一个记录页pageHandle的页头
    RM_PageHdr * GetPageHeader(const PF_PageHandle &pageHandle) const;

    // 获的一个记录页pageHandle的位图
    char * GetPageBitMap(const PF_PageHandle &pageHandle) const;

    // 根据槽号，得到一条记录的数据
    char * GetRecord(const PF_PageHandle &pageHandle, SlotNum slot) const;

    // 分配一个新页，调用关联的pfh的相关方法，返回这个新页的句柄
    // 同时更新一下必要的数据表头的信息。
    RC AllocateNewPage(PF_PageHandle &newPage);

    // 检查数据表头是否有效
    Boolean isValidFileHeader() const;

    // 得到记录的字节数
    int GetRecordSize() const;

    Boolean isOpened;                // 数据表被打开标识,从磁盘读取时，设置为TRUE,回写磁盘前设置为FALSE
    RM_FileHdr tableHeader{};        // 数据表信息头，保存在与之相关连的文件头中
    PF_FileHandle *pfh;              // 数据表对应的文件实例对象地址
    Boolean ifHeaderModified;        // header如果修改，回写时需要覆盖原来的文件头
};

/**
 * @brief 查询器类
 * OpenScan()    根据条件构建一个查询器
 * GetNextRec()  执行查询，得到下一条记录
 */
#define BEGIN_SCAN  -1              // 暂时不知道干什么用
class RM_FileScan {
public:
    RM_FileScan  ();
    ~RM_FileScan ();

    // 构建查询器
    RC OpenScan  (const RM_FileHandle &fileHandle,     // 要查询的目标数据表句柄
                  AttrType   attrType,                 // 条件查询的属性类型
                  int        attrLen,                  // 属性字段的长度
                  int        attrOffset,               // 该属性在一条记录中的位置
                  CompOp     compOp,                   // 比较操作符
                  void       *value,                   // 属性条件值
                  ClientHint pinHint = NO_HINT);
    RC GetNextRec(RM_Record &rec);                     // 查询一条记录并返回
    RC CloseScan ();                                   // 关闭查询器

private:
    // 得到每页的记录数，该字段保存在RM_PageHeader字段，也就是PF_PageHandle.pdata的开始
    int GetNumRecOnPage(PF_PageHandle& page);

    // 在所给页page中查找scanSlot的下一条记录，若达到文件尾，返回RM_EOF
    // 主要给GetNextRec调用
    RC GetNextRecord(PF_PageHandle page, RM_Record &nextRec);

    Boolean openScan;                                   // 查询迭代器是否打开

    // 查询器的参数，当执行OpenScan时，这些参数被确定
    RM_FileHandle* fileHandle;                          // 要查询的数据表的指针
    Boolean (*comparator) (void * , void *, AttrType, int);// 比较函数指针
    int attrOffset;                                     // 查询的属性偏移
    int attrLen;                                        // 属性长度
    void *value;                                        // 属性条件值
    AttrType attrType;                                  // 属性类型
    CompOp compOp;                                      // 比较操作符号，判断是否需要比较条件

    Boolean scanEnded;                                  // 主要用于GetNextRec,用来判断是否能够继续往下查询

    PageNum scanPage;                                   // 页号
    SlotNum scanSlot;                                   // 当前指向记录的槽号
    PF_PageHandle currentPH;                            // 正在查询的页
//    RM_Record currentRec;                               // 上面三项完全可以由这一项代替

    // Dictates whether to seek a record on the same page, or unpin it and
    // seek a record on the following page
//    int numRecOnPage;
//    int numSeenOnPage;
//    bool useNextPage;
//    bool hasPagePinned;
//    bool initializedValue;
};


#endif //DATABASE_RM_FILEHANDLE_H
