//
//  ix_manager.cpp
//  MicroDBMS
//
//  Created by 全俊源 on 2020/3/23.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#include <unistd.h>
#include <sys/types.h>
#include "../PF/pf.h"
#include "ix_formation.h"
#include <climits>
#include <string>
#include <sstream>
#include <cstdio>
#include "comparators.h"
#include "ix_manager.h"

#pragma mark - public functions
/**
 公有函数
 ix_manager(PF_Manager &pfm);
 ix_manager();
 RC CreateIndex(const char *fileName,
                int indexNo,
                AttrType attributeType,
                int attributeSize);
 RC DestroyIndex(const char *fileName,
                 int indexNo);
 RC OpenIndex(const char *fileName,
              int indexNo,
              ix_indexhandle &indexHandle);
 RC CloseIndex(ix_indexhandle &indexHandle);
*/

#pragma mark - 1.构造函数&析构函数
/**
 构造函数
*/
IX_Manager::IX_Manager (PF_Manager &pfm):pfManager (pfm) {
    this->pfManager = pfm;
}

/**
 析构函数
*/
IX_Manager::~IX_Manager(){}


#pragma mark - 2.创建/销毁/打开/关闭索引文件
/**
 此方法为文件 fileName 创建标号为 indexNo 的索引，该索引可以保存在文件 fileName.indexNo 里
 被索引属性的类型和长度分别在参数 attributeType 和 attributeSize 中指明
 参数fileName为文件名
 参数indexNo为索引标号
 参数attrType为被索引属性的类型
 参数attrLength为被索引属性的长度
 */
RC IX_Manager::CreateIndex (const char *fileName,
                            int indexNo,
                            AttrType attrType,
                            int attrLength) {
    //检查文件名和索引号是否有效
    if(fileName == NULL || indexNo < 0)
      return (IX_BADFILENAME);
    RC rc = 0;
    //检查属性长度和类型是否有效
    if(! indexIsValid(attrType, attrLength))
      return (IX_BADINDEXSPEC);

    // 创建索引文件
    std::string indexName;
    if((rc = getIndexFileName(fileName, indexNo, indexName)))
      return (rc);
    if((rc = pfManager.CreateFile(indexName.c_str()))){
      return (rc);
    }

    PF_FileHandle fileHandle;
    PF_PageHandle pageHeader;
    PF_PageHandle pageHandleRoot;
    
    //打开文件
    if((rc = pfManager.OpenFile(indexName.c_str(), fileHandle)))
      return (rc);
    //计算每个节点的键值和每个存储区的键值
    int nodeKeysNum = IX_IndexHandle::getNodeMaxNumKeys(attrLength);
    int bucketKeysNum = IX_IndexHandle::getBucketMaxNumKeys(attrLength);

    
    PageNum headerpage;
    PageNum rootpage;
    //创建pageHeader和pageHandleRoot
//    if((rc = fileHandle.AllocatePage(pageHeader)) || (rc = pageHeader.GetPageNum(headerpage))
//      || (rc = fileHandle.AllocatePage(pageHandleRoot)) || (rc = pageHandleRoot.GetPageNum(rootpage))){
//      return (rc);
//    }
    if((rc = fileHandle.AllocatePage(pageHeader)) || (rc = fileHandle.AllocatePage(pageHandleRoot)) ){
      return (rc);
    }
    headerpage = pageHeader.GetPageNum();
    rootpage = pageHandleRoot.GetPageNum();

    struct IndexHeader *header;
    struct IndexLeafNodeHeader *rootHeader;
    struct NodeEntry *nodeEntries;
    
    //获取pageHeader和pageHandleRoot页的数据存储于header和rootHeader中 若失败跳转到函数底关闭文件
//    if((rc = pageHeader.GetData((char *&) header)) || (rc = pageHandleRoot.GetData((char *&) rootHeader))){
//      goto closeAndExit;
//    }
    if((rc = pageHeader.GetPageData((char *&) header)) || (rc = pageHandleRoot.GetPageData((char *&) rootHeader))){
        goto closeAndExit;
    }

    //设置header属性
    header->attributeType = attrType;
    header->attributeSize = attrLength;
    header->nodeMaxKeys= nodeKeysNum;
    header->bucketMaxKeys = bucketKeysNum;
    header->nodeEntryOffset = sizeof(struct IndexInternalNodeHeader);
    header->bucketEntryOffset = sizeof(struct IndexBucketHeader);
    header->nodeKeysOffset = header->nodeEntryOffset + nodeKeysNum*sizeof(struct NodeEntry);
    header->rootPageNum = rootpage;

    //设置rootHeader属性
    rootHeader->isLeafNode = true;
    rootHeader->isEmpty = true;
    rootHeader->keysNum = 0;
    rootHeader->nextPage = NO_MORE_PAGES;
    rootHeader->prePage = NO_MORE_PAGES;
    rootHeader->firstSlotIndex = NO_MORE_SLOTS;
    rootHeader->emptySlotIndex = 0;
    nodeEntries = (struct NodeEntry *) ((char *)rootHeader + header->nodeEntryOffset);
    
    //从rootHeader开始遍历结点项
    for(int i = 0; i < header->nodeMaxKeys; i++){
        nodeEntries[i].isValid = UNOCCUPIED;
        nodeEntries[i].page = NO_MORE_PAGES;
        
        if(i == (header->nodeMaxKeys -1))
            nodeEntries[i].nextSlot = NO_MORE_SLOTS;
        else
            nodeEntries[i].nextSlot = i+1;
    }
    
    
    closeAndExit:
    RC rc2;
    //将两个页面都标记为脏，然后关闭文件
    if((rc2 = fileHandle.MarkDirty(headerpage)) || (rc2 = fileHandle.UnpinPage(headerpage)) ||
       (rc2 = fileHandle.MarkDirty(rootpage)) || (rc2 = fileHandle.UnpinPage(rootpage)) || (rc2 = pfManager.CloseFile(fileHandle)))
        return (rc2);

    return (rc);
}


/**
 该方法通过删除页式文件系统中的索引文件来删除 fileName 上标号为 indexNo 的索引
 参数fileName为文件名
 参数indexNo为索引号
*/
RC IX_Manager::DestroyIndex (const char *fileName,
                             int indexNo) {
    RC rc;
    //检查文件名是否有效 索引号是否有效
    if(fileName == NULL || indexNo < 0)
      return (IX_BADFILENAME);
    
    std::string indexName;
    //获取索引名
    if((rc = getIndexFileName(fileName, indexNo, indexName)))
      return (rc);
    //销毁文件为索引名的文件
    if((rc = pfManager.DestroyFile(indexName.c_str())))
      return (rc);
    return (0);
}


/**
 该方法通过打开页式文件系统中的索引文件来打开 fileName 上标号为 indexNo 的索引。
 如果成功执行，indexHandle 对象应指代该打开的索引
 参数fileName为文件名
 参数indexNo为索引标号
 参数indexHandle为指代该打开的索引的IX_IndexHandle
 (可通过创建多个的 indexHandle 来多次打开同一个索引，但同一时刻只有一个 indexHandle 能修改该索引)
*/
RC IX_Manager::OpenIndex (const char *fileName,
                          int indexNo,
                          IX_IndexHandle &indexHandle) {
    //检查文件名是否有效 索引号是否有效
    if(fileName == NULL || indexNo < 0){
      return (IX_BADFILENAME);
    }
    //判断indexHandle是否正在使用
    if(indexHandle.handleIsOpen == true){
      return (IX_INVALIDINDEXHANDLE);
    }
    
    RC rc = 0;
    PF_FileHandle fileHandle;
    std::string indexName;
    
    //获取索引名 并打开文件
    if((rc = getIndexFileName(fileName, indexNo, indexName)) ||
      (rc = pfManager.OpenFile(indexName.c_str(), fileHandle)))
      return (rc);

    PF_PageHandle pageHandle;
    PageNum firstpage;
    char *pData;
    
    //获取首页，获取PageNum，获取数据
//    if((rc = fileHandle.GetFirstPage(pageHandle)) || (pageHandle.GetPageNum(firstpage)) || (pageHandle.GetData(pData))){
//      fileHandle.UnpinPage(firstpage);
//      pfManager.CloseFile(fileHandle);
//      return (rc);
//    }
    if((rc = fileHandle.GetFileHdrPage(pageHandle)) || (pageHandle.GetPageData(pData))){
        firstpage = pageHandle.GetPageNum();
        fileHandle.UnpinPage(firstpage);
        pfManager.CloseFile(fileHandle);
        return (rc);
    }
    
    //设置indexHandle的属性
    struct IndexHeader * header = (struct IndexHeader *) pData;
    rc = initIndexHandle(indexHandle, fileHandle, header);
    
    RC rc2;
    //取消固定第一页
    if((rc2 = fileHandle.UnpinPage(firstpage)))
      return (rc2);
    
    //如果设置indexHandle属性失败，则关闭文件
    if(rc != 0){
      pfManager.CloseFile(fileHandle);
    }
    return (rc);
}

/**
 此方法通过关闭页式文件系统中的索引文件来关闭 indexHandle 对象指代的索引
 参数indexHandle为指定的IX_IndexHandle
*/
RC IX_Manager::CloseIndex (IX_IndexHandle &indexHandle) {
    RC rc = 0;
    PF_PageHandle pageHandle;
    PageNum pageNum;
    char *pData;

    //检查是否正在使用
    if(indexHandle.handleIsOpen == false){
        return (IX_INVALIDINDEXHANDLE);
    }

    //获取根页面的PageNum
    PageNum root = indexHandle.indexHeader.rootPageNum;
    //标记根页面被修改并取消固定
    if((rc = indexHandle.pageFileHandle.MarkDirty(root)) || (rc = indexHandle.pageFileHandle.UnpinPage(root)))
      return (rc);

    //检查header是否已被修改
    if(indexHandle.headerIsModified == true){
        //获取文件中的第一页并获取PageNum
//        if((rc = indexHandle.pageFileHandle.GetFirstPage(pageHandle)) || pageHandle.GetPageNum(pageNum))
//            return (rc);
        if((rc = indexHandle.pageFileHandle.GetFileHdrPage(pageHandle)))
            return (rc);
        pageNum = pageHandle.GetPageNum();
        //获取pageHandle中固定页的数据内容
//        if((rc = pageHandle.GetData(pData))){
//            RC rc2;
//            //取消固定
//            if((rc2 = indexHandle.pageFileHandle.UnpinPage(pageNum)))
//                return (rc2);
//            return (rc);
//        }
        if((rc = pageHandle.GetPageData(pData))){
            RC rc2;
            //取消固定
            if((rc2 = indexHandle.pageFileHandle.UnpinPage(pageNum)))
                return (rc2);
            return (rc);
        }
        //将获取的数据内容拷贝给indexHandle的索引首部indexHeader
        memcpy(pData, &indexHandle.indexHeader, sizeof(struct IndexHeader));
        //标记pageFileHandle中打开的页被修改并取消固定
        if((rc = indexHandle.pageFileHandle.MarkDirty(pageNum)) || (rc = indexHandle.pageFileHandle.UnpinPage(pageNum)))
            return (rc);
    }

    //关闭文件
    if((rc = pfManager.CloseFile(indexHandle.pageFileHandle)))
        return (rc);
    //关闭索引
    if((rc = closeIndexHandle(indexHandle)))
        return (rc);

    return (rc);
}


#pragma mark - private functions
/**
 私有函数
 bool indexIsValid(AttrType attributeType,
                   int attrLength);

 RC getIndexFileName(const char *fileName,
                     int indexNo,
                     std::string &indexname);
 
 RC initIndexHandle(ix_indexhandle &indexHandle,
                PF_FileHandle &fileHandle,
                struct indexheader *indexHeader);

 RC closeIndexHandle(ix_indexhandle &indexHandle);
*/

#pragma mark - 1.判断索引是否有效
/**
 此函数检查为attrType和attrLength传入的参数是否使其成为有效索引。 如果是，则返回true
 参数attrType为属性类型
 参数attrLength为属性长度
 */
bool IX_Manager::indexIsValid(AttrType attrType,
                              int attrLength){
    //判断属性值和长度是否匹配 若不匹配则判定无效索引
    if(attrType == INT && attrLength == 4)
        return true;
    else if(attrType == FLOAT && attrLength == 4)
        return true;
//    else if(attrType == STRING && attrLength > 0 && attrLength <= MAXSTRINGLEN)
//        return true;
    else if(attrType == CHAR && attrLength > 0 && attrLength <= MAXCHARLEN)
        return true;
    else
        return false;
}

#pragma mark - 2.由文件名和索引号获取索引名
/**
 此函数具有文件名和索引号，并以索引名的形式返回要创建的索引文件的名称
 参数fileName为文件名
 参数indexNo为索引标号
 参数indexname为索引名
 */
RC IX_Manager::getIndexFileName(const char *fileName,
                                int indexNo,
                                std::string &indexname){
    std::stringstream convert;
    convert << indexNo;
    std::string indexNum = convert.str();
    indexname = std::string(fileName);
    indexname.append(".");
    indexname.append(indexNum);
    //文件和索引号的大小限制
    if(indexname.size() > PATH_MAX || indexNum.size() > 10)
        return (IX_BADINDEXNAME);
    return (0);
}


#pragma mark - 3.设置/关闭IX_IndexHandle
/**
 此函数设置IX_IndexHandle的私有变量，使其准备引用打开的文件
 参数indexHandle为指定的IX_IndexHandle
 参数fileHandle为为指定PF_FileHandle
 参数indexHeader为指定的IndexHeader
 */
RC IX_Manager::initIndexHandle(IX_IndexHandle &indexHandle,
                               PF_FileHandle &fileHandle,
                               struct IndexHeader *indexHeader){
    RC rc = 0;
    
    //从给定的indexHandle拷贝indexheder拷贝给indexHeader
    memcpy(&indexHandle.indexHeader, indexHeader, sizeof(struct IndexHeader));

    //检查这是一个有效的索引文件
    if(! indexIsValid(indexHandle.indexHeader.attributeType, indexHandle.indexHeader.attributeSize))
        return (IX_INVALIDINDEXFILE);

    //检查标题是否有效
    if(! indexHandle.indexHeaderIsValid()){
        return (rc);
    }

    //从文件中获取根页面指向的Page
    if((rc = fileHandle.GetThisPage(indexHeader->rootPageNum, indexHandle.rootPageHandle))){
        return (rc);
    }

    //设置属性
    indexHandle.headerIsModified = false;
    indexHandle.pageFileHandle = fileHandle;
    indexHandle.handleIsOpen = true;
    
    //设置比较器
    if(indexHandle.indexHeader.attributeType == INT){
        indexHandle.comparator = compare_int;
        indexHandle.printer = print_int;
    }
    else if(indexHandle.indexHeader.attributeType == FLOAT){
        indexHandle.comparator = compare_float;
        indexHandle.printer = print_float;
    }
    else{
        indexHandle.comparator = compare_string;
        indexHandle.printer = print_string;
    }

    return (rc);
}

/**
 此函数修改文件的IX_IndexHandle以将其关闭
 参数indexHandle为被关闭的IX_IndexHandle
 */
RC IX_Manager::closeIndexHandle(IX_IndexHandle &indexHandle){
    //如果本来是关的，返回特殊值
    if(indexHandle.handleIsOpen == false)
        return (IX_INVALIDINDEXHANDLE);
    //修改isOpenHandle为关闭
    indexHandle.handleIsOpen = false;
    //正常返回
    return (0);
}
