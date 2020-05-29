//
//  ix_indexhandle.cpp
//  MicroDBMS
//
//  Created by 全俊源 on 2020/3/23.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#include <unistd.h>
#include <sys/types.h>
#include "../Header/pf.h"
#include "../Header/ix_formation.h"
#include <math.h>
#include "../Header/comparators.h"
#include <cstdio>
#include "../Header/ix_indexhandle.h"

#pragma mark - public functions
/**
  公有函数
  ix_indexhandle();
 
  ix_indexhandle();
 
  RC InsertEntry(void *pData, const RID &rid);
 
  RC DeleteEntry(void *pData, const RID &rid);
 
  RC ForcePages();
 */

#pragma mark - 1.构造函数&析构函数
//构造函数
IX_IndexHandle::IX_IndexHandle() {
    //索引句柄最初处于关闭状态
    handleIsOpen = false;
    //索引Header未被修改
    headerIsModified = false;
}

//析构函数
IX_IndexHandle::~IX_IndexHandle(){

}


#pragma mark - 2.索引的插入/删除
/**
 此方法在 ix_indexhandle 指代的索引 中插入一个新项(*pData, rid)，参数 pData 指向待插入的属性值，参数 rid 是具有该属性值的记录的标识符。
 */
RC IX_IndexHandle::InsertEntry (void *pData,
                                const RID &rid) {
    
    //仅在有效的、打开的indexHandle时允许插入
    if(!indexHeaderIsValid() || !handleIsOpen)
        return (IX_INVALIDINDEXHANDLE);

    //检索根的header
    RC rc = 0;
    struct IndexNodeHeader *rootHeader;
//    if((rc = rootPageHandle.GetData((char *&)rootHeader))){
//        return (rc);
//    }
    if((rc = rootPageHandle.GetPageData((char *&)rootHeader))){
        return (rc);
    }

    //如果根已满
    if(rootHeader->keysNum == indexHeader.nodeMaxKeys){
        //创建一个新的空根结点
        PageNum newRootPage;
        char *newRootData;
        PF_PageHandle newRootPH;
        
        //创建一个新的页面设置为结点（非叶子结点）
        if((rc = createNewNode(newRootPH, newRootPage, newRootData, false))){
            return (rc);
        }
        
        // 更新根结点
        struct IndexInternalNodeHeader *newRootHeader = (struct IndexInternalNodeHeader *)newRootData;
        newRootHeader->isEmpty = false;
        newRootHeader->firstPage = indexHeader.rootPageNum;

        int unused;
        PageNum unusedPage;
        //将当前的根结点拆分为两个结点，并使父结点成为新的根结点
        if((rc = divideNode((struct IndexNodeHeader *&)newRootData, (struct IndexNodeHeader *&)rootHeader, indexHeader.rootPageNum,
                           BEGINNING_OF_SLOTS, unused, unusedPage)))
            return (rc);
        
        //标记根结点文件被修改失败时或文件从缓冲管理器中取消固定失败时（函数正常结果返回值为0， 非零则出错）
        if((rc = pageFileHandle.MarkDirty(indexHeader.rootPageNum)) || (rc = pageFileHandle.UnpinPage(indexHeader.rootPageNum)))
            return (rc);
        
        //重置根PF_PageHandle
        rootPageHandle = newRootPH;
        indexHeader.rootPageNum = newRootPage;
        //已设置新的根页面，因此索引头部已被修改
        headerIsModified = true;

        //检索新的Root结点的内容
        struct IndexNodeHeader *useMe;
//        if((rc = newRootPH.GetData((char *&)useMe))){
//            return (rc);
//        }
        if((rc = newRootPH.GetPageData((char *&)useMe))){
            return (rc);
        }
        //插入未满的根结点
        if((rc = insertToNode(useMe, indexHeader.rootPageNum, pData, rid)))
            return (rc);
    }
    //如果root不完整，请插入其中
    else{
        if((rc = insertToNode(rootHeader, indexHeader.rootPageNum, pData, rid))){
            return (rc);
        }
    }

    //将根结点标记为脏
    if((rc = pageFileHandle.MarkDirty(indexHeader.rootPageNum)))
        return (rc);

    return (rc);
}


/**
 此方法把(*pData, rid)从 ix_indexhandle 指代的索引中删除。
 */
RC IX_IndexHandle::DeleteEntry (void *pData,
                                const RID &rid) {
    
    RC rc = 0;
    //仅在有效的、打开的indexHandle时允许删除
    if(! indexHeaderIsValid() || !handleIsOpen)
        return (IX_INVALIDINDEXHANDLE);

    //获取根结点
    struct IndexNodeHeader *rootHeader;
    //GetData函数返回正常值OK_RC值为0，若非零则出错
//    if((rc = rootPageHandle.GetData((char *&)rootHeader))){
//        return (rc);
//    }
    if((rc = rootPageHandle.GetPageData((char *&)rootHeader))){
        return (rc);
    }

    //如果根页面为空，则索引中不存在任何项
    if(rootHeader->isEmpty && (! rootHeader->isLeafNode) )
        return (IX_INVALIDENTRY);
    if(rootHeader->keysNum == 0 && rootHeader->isLeafNode)
        return (IX_INVALIDENTRY);

    //toDelete是是否删除此当前结点的标志
    bool deleteFlag = false;
    //删除，删除的过程中会根据num_keys更新是否当前结点下是否还有子结点
    if((rc = deleteFromNode(rootHeader, pData, rid, deleteFlag)))
        return (rc);

    //如果树为空，则将当前结点设置为叶子结点
    if(deleteFlag)
        rootHeader->isLeafNode = true;
    
    return (rc);
}

#pragma mark - 3.写入磁盘&输出
/**
 此方法把 ix_indexhandle 对象所在的数据页全部从内存写回磁盘。
 */
RC IX_IndexHandle::ForcePages () {
    RC rc = 0;
    //必须是打开的
    if (!handleIsOpen)
        return (IX_INVALIDINDEXHANDLE);
    //调用PF_FileHandle的拷贝回磁盘方法
    pageFileHandle.ForcePages();
    return (rc);
}


#pragma mark - 4.打印索引
/**
此方法把 ix_indexhandle 对象所在的数据页全部打印显示
*/
RC IX_IndexHandle::PrintIndex(){
    RC rc;
    PageNum leafPage;
    PF_PageHandle ph;
    struct IndexLeafNodeHeader *lheader;
    struct NodeEntry *entries;
    char *keys;
    
    if((rc = getFirstLeafPage(ph, leafPage) || (rc = pageFileHandle.UnpinPage(leafPage))))
        return (rc);
    //打印当前页
    while(leafPage != NO_MORE_PAGES){
//        if((rc = pageFileHandle.GetThisPage(leafPage, ph)) || (rc = ph.GetData((char *&)lheader)))
//            return (rc);
        if((rc = pageFileHandle.GetThisPage(leafPage, ph)) || (rc = ph.GetPageData((char *&)lheader)))
            return (rc);
        
        entries = (struct NodeEntry*) ( (char *)lheader + indexHeader.nodeEntryOffset);
        keys = (char *)lheader + indexHeader.nodeKeysOffset;
        
        int prev_idx = BEGINNING_OF_SLOTS;
        int curr_idx = lheader->firstSlotIndex;
        
        while(curr_idx != NO_MORE_SLOTS){
            printf("\n");
            printer(keys + curr_idx*indexHeader.attributeSize, indexHeader.attributeSize);
            
            if(entries[curr_idx].isValid == OCCUPIED_DUP){
                //printf("is a duplicate\n");
                PageNum bucketPage = entries[curr_idx].page;
                PF_PageHandle bucketPH;
                struct IndexBucketHeader *bheader;
                struct BucketEntry *bEntries;
                
                while(bucketPage != NO_MORE_PAGES){
//                    if((rc = pageFileHandle.GetThisPage(bucketPage, bucketPH)) || (rc = bucketPH.GetData((char *&)bheader))) {
//                        return (rc);
//                    }
                    if((rc = pageFileHandle.GetThisPage(bucketPage, bucketPH)) || (rc = bucketPH.GetPageData((char *&)bheader))) {
                        return (rc);
                    }
                    bEntries = (struct BucketEntry *) ((char *)bheader + indexHeader.bucketEntryOffset);
                    int currIdx = bheader->firstSlotIndex;
                    int prevIdx = BEGINNING_OF_SLOTS;
                    while(currIdx != NO_MORE_SLOTS){
                        //printf("currIdx: %d ", currIdx);
                        printf("rid: %d, page %d | ", bEntries[currIdx].page, bEntries[currIdx].slot);
                        prevIdx = currIdx;
                        currIdx = bEntries[prevIdx].nextSlot;
                    }
                    PageNum nextBucketPage = bheader->nextBucket;
                    if((rc = pageFileHandle.UnpinPage(bucketPage)))
                        return (rc);
                    bucketPage = nextBucketPage;
                }
            }
            else{
                printf("rid: %d, page %d | ", entries[curr_idx].page, entries[curr_idx].slot);
            }
            prev_idx = curr_idx;
            curr_idx = entries[prev_idx].nextSlot;
        }
        PageNum nextPage = lheader->nextPage;
        if(leafPage != indexHeader.rootPageNum){
            if((rc = pageFileHandle.UnpinPage(leafPage)))
                return (rc);
        }
        leafPage = nextPage;
    }
    return OK_RC;
}

#pragma mark - private functions
/**
 私有函数
 RC createNewNode(PF_PageHandle &pageHandle,
                PageNum &pageNum,
                char *& nodeData,
                bool isLeafNode);
 
 RC createNewBucket(PageNum &pageNum);

 RC divideNode(struct IndexNodeHeader *parentHeader,
            struct IndexNodeHeader *oldHeader,
            PageNum oldPage,
            int index,
            int &newKeyIndex,
        PageNum &newPageNum);

 RC insertToNode(struct IndexNodeHeader *nHeader,
                    PageNum thisNodeNum,
                    void *pData,
                    const RID &rid);
 
 RC insertToBucket(PageNum pageNum,
                const RID &rid);

 RC getPreIndex(struct IndexNodeHeader *nHeader,
                int thisIndex,
                int &prevIndex);
 
 RC getNodeInsertIndex(struct IndexNodeHeader *nHeader,
                    void* pData,
                    int& index,
                    bool& isDup);

 RC deleteFromNode(struct IndexNodeHeader *nHeader,
                void *pData,
                const RID &rid,
                bool &toDelete);
 
 RC deleteFromBucket(struct IndexBucketHeader *bHeader,
                    const RID &rid,
                    bool &deletePage,
                    RID &lastRID,
                    PageNum &nextPage);
 
 RC deleteFromLeafNode(struct IX_NodeHeader_L *nHeader,
                   void *pData,
                   const RID &rid,
                   bool &toDelete);

 bool indexHeaderIsValid() const;

 //友元类用到的辅助函数
 static int getNodeMaxNumKeys(int attributeSize);
 
 static int getBucketMaxNumKeys(int attributeSize);

 RC getFirstLeafPage(PF_PageHandle &leafPH,
                PageNum &leafPage);
 
 RC findRecordPage(PF_PageHandle &leafPH,
                PageNum &leafPage,
                void * key);
*/


#pragma mark - 1.判断Header是否是有效的
/**
 此方法根据属性的大小，键的数量和偏移量检查Header是否为有效的Header。 如果是，则返回true；否则，则返回false。
*/
bool IX_IndexHandle::indexHeaderIsValid() const{
    //检查键值的数量，小于0 则判断为无效的header
    if(indexHeader.nodeMaxKeys <= 0 || indexHeader.bucketMaxKeys <= 0){
        printf("键值为负数，无效");
        return false;
    }
    //检查偏移量，偏移量不等于相应的结构的size则判断为无效的header
    if(indexHeader.nodeEntryOffset != sizeof(struct IndexNodeHeader) ||
       indexHeader.bucketEntryOffset != sizeof(struct IndexBucketHeader)){
        printf("偏移量和结构体不匹配");
        return false;
    }
    
    //计算属性的长度
    int length = (indexHeader.nodeKeysOffset - indexHeader.nodeEntryOffset)/(indexHeader.nodeMaxKeys);
    if(length != sizeof(struct NodeEntry)){
        printf("长度和结构体不匹配");
        return false;
    }
    
    //如果总体属性大于页面的size 则必定出错，判断为无效的header
//    if((indexHeader.bucketEntryOffset + indexHeader.bucketMaxKeys * sizeof(BucketEntry) > PF_PAGE_SIZE) ||
//       indexHeader.nodeKeysOffset + indexHeader.nodeMaxKeys * indexHeader.attributeSize > PF_PAGE_SIZE)
//        return false;
    return !((indexHeader.bucketEntryOffset + indexHeader.bucketMaxKeys * sizeof(BucketEntry) > PF_PAGE_SIZE) ||
             indexHeader.nodeKeysOffset + indexHeader.nodeMaxKeys * indexHeader.attributeSize > PF_PAGE_SIZE);

}


#pragma mark - 2.创建新的结点和存储区Page,二者实现原理相似
/**
 此方法为创建一个新页面并将其设置为结点
 参数pageHandle是页码和指向其数据的指针
 参数isLeafNode是一个布尔值，表示此页面是否应该是叶子结点
 */
RC IX_IndexHandle::createNewNode(PF_PageHandle &pageHandle,
                                 PageNum &pageNum,
                                 char *& nodeData,
                                 bool isLeafNode) {
    RC rc = 0;
    //如果申请新页失败/获取页码失败 return非零值
//    if((rc = pageFileHandle.AllocatePage(pageHandle)) || (rc = pageHandle.GetPageNum(pageNum))){
//        return (rc);
//    }
    if((rc = pageFileHandle.AllocatePage(pageHandle))){
        return (rc);
    }
    pageNum = pageHandle.GetPageNum();
    //如果获取Data失败 return非零值
//    if((rc = pageHandle.GetData(nodeData)))
//        return (rc);
    if((rc = pageHandle.GetPageData(nodeData)))
        return (rc);
    
    //创建索引的NodeHeader并对其属性赋值
    struct IndexNodeHeader *nodeHeader = (struct IndexNodeHeader *)nodeData;
    nodeHeader->isLeafNode = isLeafNode;
    nodeHeader->isEmpty = true;
    nodeHeader->keysNum = 0;
    nodeHeader->pageNum1 = NO_MORE_PAGES;
    nodeHeader->pageNum2 = NO_MORE_PAGES;
    nodeHeader->firstSlotIndex = NO_MORE_SLOTS;
    nodeHeader->emptySlotIndex = 0;

    //结点项指针
    struct NodeEntry *entries = (struct NodeEntry *)((char*)nodeHeader + indexHeader.nodeEntryOffset);

    //将指针设置为freeSlotIndex列表中的链接列表
    for(int i = 0; i < indexHeader.nodeMaxKeys; i++) {
        //设置每一项的属性
        entries[i].isValid = UNOCCUPIED;
        entries[i].page = NO_MORE_PAGES;
        //nextSlot下一个插槽属性值的确定
        //末尾
        if(i == (indexHeader.nodeMaxKeys -1))
            entries[i].nextSlot = NO_MORE_SLOTS;
        //非末尾（依次链接）
        else
            entries[i].nextSlot = i+1;
    }

    return (rc);
}

/**
 此方法为创建一个新的存储区页面并将其设置为存储区
 参数page是页面中的页码
*/
RC IX_IndexHandle::createNewBucket(PageNum &pageNum) {
    char *nData;
    PF_PageHandle pageHandle;
    RC rc = 0;
    
    //如果申请新页失败/获取页码失败 return非零值
//    if((rc = pageFileHandle.AllocatePage(pageHandle)) || (rc = pageHandle.GetPageNum(pageNum))){
//        return (rc);
//    }
    if((rc = pageFileHandle.AllocatePage(pageHandle))){
        return (rc);
    }
    pageNum = pageHandle.GetPageNum();

    //如果获取Data失败 return非零值 并且在获取失败后，缓冲池取消固定页面
//    if((rc = pageHandle.GetData(nData))){
//        RC rc2;
//        if((rc2 = pageFileHandle.UnpinPage(pageNum)))
//            return (rc2);
//        return (rc);
//    }
    if((rc = pageHandle.GetPageData(nData))){
        RC rc2;
        if((rc2 = pageFileHandle.UnpinPage(pageNum)))
            return (rc2);
        return (rc);
    }
    
    //创建BucketHeader并对其属性赋值
    struct IndexBucketHeader *bucketHeader = (struct IndexBucketHeader *) nData;
    bucketHeader->keysNum = 0;
    bucketHeader->firstSlotIndex = NO_MORE_SLOTS;
    bucketHeader->emptySlotIndex = 0;
    bucketHeader->nextBucket = NO_MORE_PAGES;

    //存储区项
    struct BucketEntry *entries = (struct BucketEntry *)((char *)bucketHeader + indexHeader.bucketEntryOffset);
    
    //将插槽指针设置为freeSlotIndex列表中的链接列表
    for(int i = 0; i < indexHeader.bucketMaxKeys; i++){
        //nextSlot属性值的确定
        //末尾
        if(i == (indexHeader.bucketMaxKeys -1))
            entries[i].nextSlot = NO_MORE_SLOTS;
        //非末尾
        else
            entries[i].nextSlot = i+1;
    }
    
    //如果标记页面被修改操作失败/取消固定页面操作失败 return非零值
    if( (rc = pageFileHandle.MarkDirty(pageNum)) || (rc = pageFileHandle.UnpinPage(pageNum)))
        return (rc);

    //正常返回
    return (rc);
}



#pragma mark - 3.传入当前值和索引值找到前一个找到索引 & 找到给定值应该插入的索引
/**
 此方法为给定首部和索引，将返回前一个索引
 参数nHeader是给定的header
 参数thisIndex是给定的索引
 参数prevIndex是前一个索引
*/
RC IX_IndexHandle::getPreIndex(struct IndexNodeHeader *nHeader,
                                 int thisIndex,
                                 int &prevIndex) {
    
    struct NodeEntry *nodEntries = (struct NodeEntry *)((char *)nHeader + indexHeader.nodeEntryOffset);
    //起始位置
    int preIndex = BEGINNING_OF_SLOTS;
    //第一个有效的插槽
    int currIndex = nHeader->firstSlotIndex;
    //当当前索引与传入的thisIndex的不同
    while(currIndex != thisIndex){
        //更新索引
        preIndex = currIndex;
        currIndex = nodEntries[preIndex].nextSlot;
    }
    //找到当前给定的thisIndex的前一个索引
    prevIndex = preIndex;
    return (0);
}


/**
 此方法要找到给定的键值对应的索引位置
 参数nHeader是给定的头部
 参数pData是指向给定的键值的指针
 参数index是最终要返回的索引
 参数isDuplicate是s当前插入值是否为重复项的标识位
*/
RC IX_IndexHandle::getNodeInsertIndex(struct IndexNodeHeader *nHeader,
                                       void *pData,
                                       int& index,
                                       bool& isDuplicate){
    
    //初始化结点项链表和键值链表
    struct NodeEntry *nodeEntries = (struct NodeEntry *)((char *)nHeader + indexHeader.nodeEntryOffset);
    char *keys = ((char *)nHeader + indexHeader.nodeKeysOffset);

    //起始位置
    int preIndex = BEGINNING_OF_SLOTS;
    //第一个有效的插槽
    int currentIndex = nHeader->firstSlotIndex;
   
    //重复项
    isDuplicate = false;
    //检索，直到在结点中找到的键大于给定的pData
    while(currentIndex != NO_MORE_SLOTS){
        //当前值
        char *value = keys + indexHeader.attributeSize * currentIndex;
        //比较当前值和给定值
        int compared = comparator(pData, (void*) value, indexHeader.attributeSize);
        //如果二者相等，则更新重复项标志位
        if(compared == 0)
            isDuplicate = true;
        //给定的值小于当前值，跳出循环
        if(compared < 0)
            break;
        //更新索引
        preIndex = currentIndex;
        currentIndex = nodeEntries[preIndex].nextSlot;
    }
    //更新要返回的索引
    index = preIndex;
    return (0);
}


#pragma mark - 4.拆分给定结点(此函数逻辑相对复杂)
/**
 此方法为 ix_indexhandle 中拆分结点函数
 参数parentHeader是父结点的header
 参数oldHeader是要被拆分的结点的header
 参数oldPage是要被拆分的旧结点的PageNum
 参数index是在父结点中插入新结点的索引
 参数newKeyIndex是指向新结点的第一个键值的索引
 参数newPageNum是新结点的PageNum
*/
RC IX_IndexHandle::divideNode(struct IndexNodeHeader *parentHeader,
                             struct IndexNodeHeader *oldHeader,
                             PageNum oldPage,
                             int index,
                             int & newKeyIndex,
                             PageNum &newPageNum) {
    RC rc = 0;
    //标识位
    bool isLeafNode = false;
    //创建新页面并获取其header
    PageNum newPage;
    struct IndexNodeHeader *newHeader;
    PF_PageHandle newPageHandle;
    
    //被拆分结点是叶子结点
    if(oldHeader->isLeafNode == true){
        isLeafNode = true;
    }
    
    //如果创建新结点失败 return非零值
    if((rc = createNewNode(newPageHandle, newPage, (char *&)newHeader, isLeafNode)))
        return (rc);
    
    
    //成功创建新结点，返回新的页码
    newPageNum = newPage;

    //检索指向所有结点内容的指针
    struct NodeEntry *parentEntries = (struct NodeEntry *) ((char *)parentHeader + indexHeader.nodeEntryOffset);
    struct NodeEntry *oldEntries = (struct NodeEntry *) ((char *)oldHeader + indexHeader.nodeEntryOffset);
    struct NodeEntry *newEntries = (struct NodeEntry *) ((char *)newHeader + indexHeader.nodeEntryOffset);
    char *parentKeys = (char *)parentHeader + indexHeader.nodeKeysOffset;
    char *newKeys = (char *)newHeader + indexHeader.nodeKeysOffset;
    char *oldKeys = (char *)oldHeader + indexHeader.nodeKeysOffset;

    //将前header.maxKeys_N/2值保留在旧结点中
    int prev_frontIndex = BEGINNING_OF_SLOTS;
    int curr_frontIndex = oldHeader->firstSlotIndex;
    
    //更新被拆分结点的信息
    for(int i = 0; i < indexHeader.nodeMaxKeys/2 ; i++){
        prev_frontIndex = curr_frontIndex;
        curr_frontIndex = oldEntries[prev_frontIndex].nextSlot;
    }
    oldEntries[prev_frontIndex].nextSlot = NO_MORE_SLOTS;

    //在父结点中用来指向正在创建的新结点的键
    char *parentKey = oldKeys + curr_frontIndex*indexHeader.attributeSize;
  
    //如果不拆分叶结点，则在以下位置更新firstPageNum指针
    if(!isLeafNode){
        //创建新内部结点的Header，并对属性赋值
        struct IndexInternalNodeHeader *newIHeader = (struct IndexInternalNodeHeader *)newHeader;
        newIHeader->firstPage = oldEntries[curr_frontIndex].page;
        newIHeader->isEmpty = false;
        prev_frontIndex = curr_frontIndex;
        curr_frontIndex = oldEntries[prev_frontIndex].nextSlot;
        oldEntries[prev_frontIndex].nextSlot = oldHeader->emptySlotIndex;
        oldHeader->emptySlotIndex = prev_frontIndex;
        oldHeader->keysNum--;
    }

    //将剩余的header.maxKeys_N/2值移动到新结点中
    int prev_backIndex = BEGINNING_OF_SLOTS;
    int curr_backIndex = newHeader->emptySlotIndex;
    while(curr_frontIndex != NO_MORE_SLOTS){
        //新的结点项属性更新
        newEntries[curr_backIndex].page = oldEntries[curr_frontIndex].page;
        newEntries[curr_backIndex].slot = oldEntries[curr_frontIndex].slot;
        newEntries[curr_backIndex].isValid = oldEntries[curr_frontIndex].isValid;
        //拷贝（拷贝的字节数为indexHeader.attrLength）
        memcpy(newKeys + curr_backIndex*indexHeader.attributeSize, oldKeys + curr_frontIndex*indexHeader.attributeSize, indexHeader.attributeSize);
        //首个 特殊处理
        if(prev_backIndex == BEGINNING_OF_SLOTS){
            newHeader->emptySlotIndex = newEntries[curr_backIndex].nextSlot;
            newEntries[curr_backIndex].nextSlot = newHeader->firstSlotIndex;
            newHeader->firstSlotIndex = curr_backIndex;
        }
        else{
            newHeader->emptySlotIndex = newEntries[curr_backIndex].nextSlot;
            newEntries[curr_backIndex].nextSlot = newEntries[prev_backIndex].nextSlot;
            newEntries[prev_backIndex].nextSlot = curr_backIndex;
        }
        prev_backIndex = curr_backIndex;
        //更新插入索引
        curr_backIndex = newHeader->emptySlotIndex;

        prev_frontIndex = curr_frontIndex;
        curr_frontIndex = oldEntries[prev_frontIndex].nextSlot;
        oldEntries[prev_frontIndex].nextSlot = oldHeader->emptySlotIndex;
        oldHeader->emptySlotIndex = prev_frontIndex;
        oldHeader->keysNum--;
        newHeader->keysNum++;
    }

    //在参数指定的索引处插入父键
    int location = parentHeader->emptySlotIndex;
    memcpy(parentKeys + location * indexHeader.attributeSize, parentKey, indexHeader.attributeSize);
    
    //返回指向新结点的插槽位置
    newKeyIndex = location;
    parentEntries[location].page = newPage;
    parentEntries[location].isValid = OCCUPIED_NEW;
    if(index == BEGINNING_OF_SLOTS){
        parentHeader->emptySlotIndex = parentEntries[location].nextSlot;
        parentEntries[location].nextSlot = parentHeader->firstSlotIndex;
        parentHeader->firstSlotIndex = location;
    }
    else{
        parentHeader->emptySlotIndex = parentEntries[location].nextSlot;
        parentEntries[location].nextSlot = parentEntries[index].nextSlot;
        parentEntries[index].nextSlot = location;
    }
    parentHeader->keysNum++;

    //如果是叶子结点，则更新指向上一个和下一个叶子结点的页面指针
    if(isLeafNode){
        struct IndexLeafNodeHeader *newLHeader = (struct IndexLeafNodeHeader *) newHeader;
        struct IndexLeafNodeHeader *oldLHeader = (struct IndexLeafNodeHeader *) oldHeader;
        newLHeader->nextPage = oldLHeader->nextPage;
        newLHeader->prePage = oldPage;
        oldLHeader->nextPage = newPage;
        if(newLHeader->nextPage != NO_MORE_PAGES){
            PF_PageHandle nextPageHandle;
            struct IndexLeafNodeHeader *nextHeader;
            //获取nextPage失败/获取Data失败 返回非零值
//            if((rc = pageFileHandle.GetThisPage(newLHeader->nextPage, nextPageHandle)) || (nextPageHandle.GetData((char *&)nextHeader)))
//                return (rc);
            if((rc = pageFileHandle.GetThisPage(newLHeader->nextPage, nextPageHandle)) || (nextPageHandle.GetPageData((char *&)nextHeader)))
                return (rc);
            //更新nextHeader
            nextHeader->prePage = newPage;
            //将新页面标记为脏页面，然后取消固定
            if((rc = pageFileHandle.MarkDirty(newLHeader->nextPage)) || (rc = pageFileHandle.UnpinPage(newLHeader->nextPage)))
                return (rc);
        }
    }

    //将新页面标记为脏页面，然后取消固定
    if((rc = pageFileHandle.MarkDirty(newPage))||(rc = pageFileHandle.UnpinPage(newPage))){
        return (rc);
    }
    return (rc);
}



#pragma mark - 5.向结点或存储区插入,未满的结点 如果结点已满 则需要在调用此函数之前先调用拆分结点函数,
/**
 此方法为将Value和RID插入
 参数nHeader是给定的header
 参数thisNodeNum是给定的页码
 参数pData是要插入的value
 参数rid是要插入的RID
*/
RC IX_IndexHandle::insertToNode(struct IndexNodeHeader *nHeader,
                                PageNum thisNodeNum,
                                void *pData,
                                const RID &rid) {
    RC rc = 0;

    //检索此结点的内容
    struct NodeEntry *nodeEntries = (struct NodeEntry *) ((char *)nHeader + indexHeader.nodeEntryOffset);
    char *keys = (char *)nHeader + indexHeader.nodeKeysOffset;

    //如果它是叶结点，则插入其中
    if(nHeader->isLeafNode) {
        int prevInsertIndex = BEGINNING_OF_SLOTS;
        //是否为重复项标志位
        bool isDuplicate = false;
        //成功找到合适的索引，若失败则return非零值（函数内部更新isDuplicate标志位）
        if((rc = getNodeInsertIndex(nHeader, pData, prevInsertIndex, isDuplicate)))
            return (rc);
        
        //如果不是重复项，则为其插入新的索引，并更新插槽和页面值
        if(!isDuplicate){
            //找到空闲插槽
            int index = nHeader->emptySlotIndex;
            memcpy(keys + indexHeader.attributeSize * index, (char *)pData, indexHeader.attributeSize);
            //将其标记为单个项
            nodeEntries[index].isValid = OCCUPIED_NEW;
            //获取当前记录的pageNum 和 slotNum  失败return非零值
//            if((rc = rid.GetPageNum(nodeEntries[index].page)) || (rc = rid.GetSlotNum(nodeEntries[index].slot)))
//                return (rc);
            nodeEntries[index].page = rid.GetPageNum();
            nodeEntries[index].slot = rid.GetSlotNum();
            
            //空闲插槽指针后移（当前指针指向的位置存放新插入的索引）
            nHeader->emptySlotIndex = nodeEntries[index].nextSlot;
            //修改给定的header的属性
            nHeader->isEmpty = false;
            //键值加一
            nHeader->keysNum++;
            
            //如果当前插入位置是最开始（第一个）
            if(prevInsertIndex == BEGINNING_OF_SLOTS){
                nodeEntries[index].nextSlot = nHeader->firstSlotIndex;
                nHeader->firstSlotIndex = index;
            }
            //从中间插入
            else{
                nodeEntries[index].nextSlot = nodeEntries[prevInsertIndex].nextSlot;
                nodeEntries[prevInsertIndex].nextSlot = index;
            }
        }

        //如果重复，则添加新页面，或者将其添加到现有存储区中
        else {
            PageNum bucketPage;
            //是重复项且当前isValid标识位是OCCUPIED_NEW
            if (isDuplicate && nodeEntries[prevInsertIndex].isValid == OCCUPIED_NEW){
                //创建存储区 失败return 非零值
                if((rc = createNewBucket(bucketPage)))
                    return (rc);
                //修改标识位isValid 标记其有重复项
                nodeEntries[prevInsertIndex].isValid = OCCUPIED_DUP;
                //新的RID
                RID rid2(nodeEntries[prevInsertIndex].page, nodeEntries[prevInsertIndex].slot);
                //插入此新RID，并将现有项插入存储区
                if((rc = insertToBucket(bucketPage, rid2)) || (rc = insertToBucket(bucketPage, rid)))
                    return (rc);
                //页面现在指向存储区
                nodeEntries[prevInsertIndex].page = bucketPage;
            }
            //当前isValid标识位不是OCCUPIED_NEW，则可以直接插入
            else{
                bucketPage = nodeEntries[prevInsertIndex].page;
                //插入现有的存储区
                if((rc = insertToBucket(bucketPage, rid)))
                    return (rc);
            }
      
        }

    }
    //非叶子结点，内部结点获取其内容，并找到插入位置
    else{
        //创建内部结点
        struct IndexInternalNodeHeader *nIHeader = (struct IndexInternalNodeHeader *)nHeader;
        PageNum nextNodePage;
        int prevInsertIndex = BEGINNING_OF_SLOTS;
        //重复标志位
        bool isDuplicate;
        
        //如果可以找不到合适的索引插入处 return 非零值
        if((rc = getNodeInsertIndex(nHeader, pData, prevInsertIndex, isDuplicate)))
            return (rc);
        
        //修改nextNodePage
        //如果找到的插入处是在第一个的位置
        if(prevInsertIndex == BEGINNING_OF_SLOTS)
            nextNodePage = nIHeader->firstPage;
        else {
            nextNodePage = nodeEntries[prevInsertIndex].page;
        }

        //阅读下一个Page来完成插入
        PF_PageHandle nextNodePageHandle;
        struct IndexNodeHeader *nextNodeHeader;
        int newKeyIndex;
        PageNum newPageNum;
        
        //获取nextNodePage指向的页面 并且 获取该页面的内容
//        if((rc = pageFileHandle.GetThisPage(nextNodePage, nextNodePageHandle)) || (rc = nextNodePageHandle.GetData((char *&)nextNodeHeader)))
//            return (rc);
        if((rc = pageFileHandle.GetThisPage(nextNodePage, nextNodePageHandle)) || (rc = nextNodePageHandle.GetPageData((char *&)nextNodeHeader)))
            return (rc);
        //如果下一个结点已满，则需要拆分该结点
        if(nextNodeHeader->keysNum == indexHeader.nodeMaxKeys){
            //拆分结点
            if((rc = divideNode(nHeader, nextNodeHeader, nextNodePage, prevInsertIndex, newKeyIndex, newPageNum)))
                return (rc);
            
            //计算新拆分的结点的第一个键的值（value）
            char *value = keys + newKeyIndex*indexHeader.attributeSize;

            //比较值的大小
            int compared = comparator(pData, (void *)value, indexHeader.attributeSize);
            //如果要插入的值大于新拆分的结点的第一个键值
            if(compared >= 0){
                PageNum nextPage = newPageNum;
                //将原来的页码指示的Page标记为脏并取消固定（不再对其进行操作）
                if((rc = pageFileHandle.MarkDirty(nextNodePage)) || (rc = pageFileHandle.UnpinPage(nextNodePage)))
                    return (rc);
                //获取newPageNum指向的页面 并且 获取该页面的内容
//                if((rc = pageFileHandle.GetThisPage(nextPage, nextNodePageHandle)) || (rc = nextNodePageHandle.GetData((char *&) nextNodeHeader)))
//                    return (rc);
                if((rc = pageFileHandle.GetThisPage(nextPage, nextNodePageHandle)) || (rc = nextNodePageHandle.GetPageData((char *&) nextNodeHeader)))
                    return (rc);
                //更新需要操作的Page的页码
                nextNodePage = nextPage;
            }
        }
        //插入结点
        if((rc = insertToNode(nextNodeHeader, nextNodePage, pData, rid)))
            return (rc);
        //将新页面（插入结点的）标记为脏并取消固定
        if((rc = pageFileHandle.MarkDirty(nextNodePage)) || (rc = pageFileHandle.UnpinPage(nextNodePage)))
            return (rc);
    }
    return (rc);
}


/**
 将RID插入与某个键相关联的存储区中
 参数pageNum为要插入的Page的页码
 参数rid为待插入的RID值
*/
RC IX_IndexHandle::insertToBucket(PageNum pageNum,
                                 const RID &rid){
    RC rc = 0;
    
    //从rid对象获取页面和插槽号
    PageNum ridPage;
    SlotNum ridSlot;
//    if((rc = rid.GetPageNum(ridPage)) || (rc = rid.GetSlotNum(ridSlot)))
//        return (rc);
    ridPage = rid.GetPageNum();
    ridSlot = rid.GetSlotNum();

    //用于在存储区中搜索空位
    bool notEndFlag = true;
    PageNum currPage = pageNum;
    PF_PageHandle bucketPageHandle;
    struct IndexBucketHeader *bucketHeader;
    
    while(notEndFlag){
        //获取待插入页（pageNum指向的Page）的内容
//        if((rc = pageFileHandle.GetThisPage(currPage, bucketPageHandle)) || (rc = bucketPageHandle.GetData((char *&)bucketHeader))){
//            return (rc);
//        }
        if((rc = pageFileHandle.GetThisPage(currPage, bucketPageHandle)) || (rc = bucketPageHandle.GetPageData((char *&)bucketHeader)))
            return rc;
        //尝试在数据库中查找项
        struct BucketEntry *bucketEntries = (struct BucketEntry *)((char *)bucketHeader + indexHeader.bucketEntryOffset);
        int prevIndex = BEGINNING_OF_SLOTS;
        int currentIndex = bucketHeader->firstSlotIndex;
        
        //当前索引指向的插槽还有值
        while(currentIndex != NO_MORE_SLOTS){
            //如果发现重复的项，则返回错误
            if(bucketEntries[currentIndex].page == ridPage && bucketEntries[currentIndex].slot == ridSlot){
                RC rc2 = 0;
                //取消固定
                if((rc2 = pageFileHandle.UnpinPage(currPage)))
                    return (rc2);
                return (IX_DUPLICATEENTRY);
            }
            //此时currentIndex值为NO_MORE_SLOTS 其后不再指向有效值
            prevIndex = currentIndex;
            currentIndex = bucketEntries[prevIndex].nextSlot;
        }
        //如果这是存储区中的最后一个，并且已满，则创建一个新存储区
        if(bucketHeader->nextBucket == NO_MORE_PAGES && bucketHeader->keysNum == indexHeader.bucketMaxKeys){
            //结束搜索
            notEndFlag = false;
            PageNum newBucketPage;
            PF_PageHandle newBucketPH;
            
            //创建一个新的存储区
            if((rc = createNewBucket(newBucketPage)))
                return (rc);
            //将新创建的存储区链接到bucketHeader上
            bucketHeader->nextBucket = newBucketPage;
            //标记上一个存储区修改 并取消固定
            if((rc = pageFileHandle.MarkDirty(currPage)) || (rc = pageFileHandle.UnpinPage(currPage)))
                return (rc);
            
            //获取新存储区的内容
            currPage = newBucketPage;
//            if((rc = pageFileHandle.GetThisPage(currPage, bucketPageHandle)) || (rc = bucketPageHandle.GetData((char *&)bucketHeader)))
//                return (rc);
            if((rc = pageFileHandle.GetThisPage(currPage, bucketPageHandle)) || (rc = bucketPageHandle.GetPageData((char *&)bucketHeader)))
                return (rc);
            bucketEntries = (struct BucketEntry *)((char *)bucketHeader + indexHeader.bucketEntryOffset);
        }
        
        //如果它是存储区中的最后一个，则将值插入(存在两种情况：1.是最后一个存储区并且未满；2.之前最后一个已满的存储，当前最后一个是新建的存储区)
        if(bucketHeader->nextBucket == NO_MORE_PAGES){
            //结束搜索
            notEndFlag = false;
            //找到第一个空闲插槽
            int location = bucketHeader->emptySlotIndex;
            //插入RID
            bucketEntries[location].slot = ridSlot;
            bucketEntries[location].page = ridPage;
            //更新空闲插槽
            bucketHeader->emptySlotIndex = bucketEntries[location].nextSlot;
            bucketEntries[location].nextSlot = bucketHeader->firstSlotIndex;
            bucketHeader->firstSlotIndex = location;
            bucketHeader->keysNum++;
        }

        //将currPage指针更新为序列中的下一个存储区
        PageNum nextPage = bucketHeader->nextBucket;
        if((rc = pageFileHandle.MarkDirty(currPage)) || (rc = pageFileHandle.UnpinPage(currPage)))
            return (rc);
        currPage = nextPage;
    }
    return (0);
}



#pragma mark - 6.从内部结点,叶子和存储区中删除
/**
 此方法从给定结点头的结点中删除项。
 它返回一个指示当前结点是否为空的布尔值toDelete，以信号通知调用方删除该结点。
 参数nHeader是给定结点头
 参数pData是给定值
 参数rid是要被删除的值的RID
 参数toDelete是当前节点是否为空的标识位
*/
RC IX_IndexHandle::deleteFromNode(struct IndexNodeHeader *nHeader,
                                          void *pData,
                                          const RID &rid,
                                          bool &toDelete){
    RC rc = 0;
    //当前结点是否为空的标识位
    toDelete = false;
    //如果是叶结点，则从那里删除它
    if(nHeader->isLeafNode){
        //调用删除叶子结点函数实现
        if((rc = deleteFromLeafNode((struct IndexLeafNodeHeader *)nHeader, pData, rid, toDelete)))
            return (rc);
        
    }
    //是内部结点，找到合适的子结点，然后从那里删除
    else{
        //索引
        int preIndex, currIndex;
        //重复项的标识位
        bool isDuplicate;
        //找到正确的索引
        if((rc = getNodeInsertIndex(nHeader, pData, currIndex, isDuplicate)))
            return (rc);
        
        //设置内部结点头部和结点链表
        struct IndexInternalNodeHeader *internalNodeHeader = (struct IndexInternalNodeHeader *)nHeader;
        struct NodeEntry *nodeEntries = (struct NodeEntry *)((char *)nHeader + indexHeader.nodeEntryOffset);
    
        PageNum nextNodePage;
        bool useFirstPage = false;
        
        //使用内部结点中的第一个插槽作为包含此值的子集
        //当前索引是起始
        if(currIndex == BEGINNING_OF_SLOTS){
            useFirstPage = true;
            nextNodePage = internalNodeHeader->firstPage;
            preIndex = currIndex;
        }
        //当前索引非起始索引，要检索此索引之前的页面索引，以进行删除
        else{
            //找前一个索引
            if((rc = getPreIndex(nHeader, currIndex, preIndex)))
                return (rc);
            nextNodePage = nodeEntries[currIndex].page;
        }

        
        PF_PageHandle nextNodePageHandle;
        struct IndexNodeHeader *nextHeader;
        //获取下一页面内容
//        if((rc = pageFileHandle.GetThisPage(nextNodePage, nextNodePageHandle)) || (rc = nextNodePageHandle.GetData((char *&)nextHeader)))
//            return (rc);
        if((rc = pageFileHandle.GetThisPage(nextNodePage, nextNodePageHandle)) || (rc = nextNodePageHandle.GetPageData((char *&)nextHeader)))
            return (rc);
        //用于删除页面的标识位
        bool toDeleteNext = false;
        //删除nextHeader中当前值
        rc = deleteFromNode(nextHeader, pData, rid, toDeleteNext);

        RC rc2 = 0;
        //标识nextNodePage修改并取消锁定
        if((rc2 = pageFileHandle.MarkDirty(nextNodePage)) || (rc2 = pageFileHandle.UnpinPage(nextNodePage)))
            return (rc2);

        //如果找不到该项
        if(rc == IX_INVALIDENTRY)
            return (rc);

        //如果项已成功删除，检查是否删除此子结点
        if(toDeleteNext){
            //如果是，则销毁页面
            if((rc = pageFileHandle.DisposePage(nextNodePage)))
                return (rc);
            
            //如果删除的页面是第一页，则将第二页放入firstPage插槽
            if(useFirstPage == false){
                //是起始位置
                if(preIndex == BEGINNING_OF_SLOTS)
                    nHeader->firstSlotIndex = nodeEntries[currIndex].nextSlot;
                else
                    nodeEntries[preIndex].nextSlot = nodeEntries[currIndex].nextSlot;
                
                //更新插槽值
                nodeEntries[currIndex].nextSlot = nHeader->emptySlotIndex;
                nHeader->emptySlotIndex = currIndex;
            }
            //如果删除的不是第一页，只需从插槽指针序列中删除此页面
            else{
                int firstslot = nHeader->firstSlotIndex;
                nHeader->firstSlotIndex = nodeEntries[firstslot].nextSlot;
                internalNodeHeader->firstPage = nodeEntries[firstslot].page;
                nodeEntries[firstslot].nextSlot = nHeader->emptySlotIndex;
                nHeader->emptySlotIndex = firstslot;
            }
            
            //更新该结点头部的键值数
            //如果没有更多的键，而我们只是删除了第一页，则返回删除结点的标识位为true
            if(nHeader->keysNum == 0){
                nHeader->isEmpty = true;
                toDelete = true;
            }
            else
                nHeader->keysNum--;
        }
    }
    return (rc);
}


/**
 此方法从给定头部的叶子中删除项。
 返回toDelete这个叶子结点是否为空决定是否删除它
 参数nHeader是给定结点头
 参数pData是给定值
 参数rid是要被删除的值的RID
 参数toDelete是当前节点是否为空的标识位
*/
RC IX_IndexHandle::deleteFromLeafNode(struct IndexLeafNodeHeader *nHeader,
                                      void *pData,
                                      const RID &rid,
                                      bool &toDelete){
    RC rc = 0;
    //索引
    int prevIndex, currIndex;
    //重复项的标识位
    bool isDuplicate;
    
    //找到正确的索引
    if((rc = getNodeInsertIndex((struct IndexNodeHeader *)nHeader, pData, currIndex, isDuplicate)))
        return (rc);
    //如果该项存在，则叶子中应存在一个键
    if(isDuplicate == false)
        return (IX_INVALIDENTRY);

    //设置结点链表和键值
    struct NodeEntry *nodeEntries = (struct NodeEntry *)((char *)nHeader + indexHeader.nodeEntryOffset);
    char *key = (char *)nHeader + indexHeader.nodeKeysOffset;

    //当前索引是有效索引的开头
    if(currIndex == nHeader->firstSlotIndex)
        prevIndex = currIndex;
    else{
        //找到前一个索引
        if((rc = getPreIndex((struct IndexNodeHeader *)nHeader, currIndex, prevIndex)))
            return (rc);
    }

    //如果只有单个项，从叶子中将其删除
    if(nodeEntries[currIndex].isValid == OCCUPIED_NEW){
        PageNum ridPage;
        SlotNum ridSlot;
        //获取当前RID的PageNum 和 SlotNum
//        if((rc = rid.GetPageNum(ridPage)) || (rc = rid.GetSlotNum(ridSlot)))
//            return (rc);
        ridPage = rid.GetPageNum();
        ridSlot = rid.GetSlotNum();
        
        //如果此RID和键值不匹配，则该项不存在，返回IX_INVALIDENTRY
        int compare = comparator((void*)(key + indexHeader.attributeSize*currIndex), pData, indexHeader.attributeSize);
        if(ridPage != nodeEntries[currIndex].page || ridSlot != nodeEntries[currIndex].slot || compare != 0 )
            return (IX_INVALIDENTRY);

        //有效指针的开头删除
        if(currIndex == nHeader->firstSlotIndex){
            nHeader->firstSlotIndex = nodeEntries[currIndex].nextSlot;
        }
        //有效指针的中间删除
        else
            nodeEntries[prevIndex].nextSlot = nodeEntries[currIndex].nextSlot;
        
        //更新结点链表的currIndex处的值
        nodeEntries[currIndex].nextSlot = nHeader->emptySlotIndex;
        nHeader->emptySlotIndex = currIndex;
        nodeEntries[currIndex].isValid = UNOCCUPIED;
        
        //更新键值数
        nHeader->keysNum--;
    }
    
    //如果有重复项，则将其从相应的存储区中删除
    else if(nodeEntries[currIndex].isValid == OCCUPIED_DUP){
        PageNum bucketNum = nodeEntries[currIndex].page;
        PF_PageHandle bucketPageHandle;
        struct IndexBucketHeader *bHeader;
        bool deletePage = false;
        RID lastRID;
        PageNum nextBucketNum;
        
        //获取bucketNum指定的页面和其数据内容
//        if((rc = pageFileHandle.GetThisPage(bucketNum, bucketPageHandle)) || (rc = bucketPageHandle.GetData((char *&)bHeader)))
//            return (rc);
        if((rc = pageFileHandle.GetThisPage(bucketNum, bucketPageHandle)) || (rc = bucketPageHandle.GetPageData((char *&)bHeader)))
            return (rc);
        
        //删除存储区
        rc = deleteFromBucket(bHeader, rid, deletePage, lastRID, nextBucketNum);
        
        RC rc2 = 0;
        //标记bucketNum页被修改并取消锁定
        if((rc2 = pageFileHandle.MarkDirty(bucketNum)) || (rc = pageFileHandle.UnpinPage(bucketNum)))
            return (rc2);

        //如果存储区中不存在它，则返回IX_INVALIDENTRY
        if(rc == IX_INVALIDENTRY)
            return (IX_INVALIDENTRY);

        //如果需要删除存储区
        if(deletePage){
            //销毁页面
            if((rc = pageFileHandle.DisposePage(bucketNum) ))
                return (rc);
            //如果没有更多的存储区，则将最后一个RID放在叶子页中，并将isValid标志更新为OCCUPIED_NEW
            if(nextBucketNum == NO_MORE_PAGES){
                nodeEntries[currIndex].isValid = OCCUPIED_NEW;
                //获取RID的PageNum和SlotNum
//                if((rc = lastRID.GetPageNum(nodeEntries[currIndex].page)) ||
//                   (rc = lastRID.GetSlotNum(nodeEntries[currIndex].slot)))
//                    return (rc);
                nodeEntries[currIndex].page = lastRID.GetPageNum();
                nodeEntries[currIndex].slot = lastRID.GetSlotNum();
            }
            //否则，将存储区指针设置为下一个存储区
            else
                nodeEntries[currIndex].page = nextBucketNum;
        }
    }
    //如果叶子现在为空，返回可以删除的标识位
    if(nHeader->keysNum == 0){
        toDelete = true;
        //更新其上一个和下一个邻居的叶子指针
        PageNum prePage = nHeader->prePage;
        PageNum nextPage = nHeader->nextPage;
        PF_PageHandle leafPageHandle;
        struct IndexLeafNodeHeader *leafHeader;
        if(prePage != NO_MORE_PAGES){
//            if((rc = pageFileHandle.GetThisPage(prePage, leafPageHandle))|| (rc = leafPageHandle.GetData((char *&)leafHeader)) )
//                return (rc);
            if((rc = pageFileHandle.GetThisPage(prePage, leafPageHandle))|| (rc = leafPageHandle.GetPageData((char *&)leafHeader)) )
                return (rc);
            leafHeader->nextPage = nextPage;
            if((rc = pageFileHandle.MarkDirty(prePage)) || (rc = pageFileHandle.UnpinPage(prePage)))
                return (rc);
        }
        if(nextPage != NO_MORE_PAGES){
//            if((rc = pageFileHandle.GetThisPage(nextPage, leafPageHandle))|| (rc = leafPageHandle.GetData((char *&)leafHeader)) )
//                return (rc);
            if((rc = pageFileHandle.GetThisPage(nextPage, leafPageHandle))|| (rc = leafPageHandle.GetPageData((char *&)leafHeader)) )
                return (rc);
            leafHeader->prePage = prePage;
            if((rc = pageFileHandle.MarkDirty(nextPage)) || (rc = pageFileHandle.UnpinPage(nextPage)))
                return (rc);
        }
    }
    return (0);
}


/**
 此方法给定一个RID和一个bucketHeader，将从存储区中删除该RID
 它返回有关是否删除存储区的标识位
 参数bHeader为给定的IndexBucketHeader
 参数rid为给定的RID
 参数deletePage为否删除存储区的标识位
 参数lastRID为存储区的最后一个RID
 参数nextPage为存储区指向的下一个存储区的PageNum
*/
RC IX_IndexHandle::deleteFromBucket(struct IndexBucketHeader *bHeader,
                                    const RID &rid,
                                    bool &deletePage,
                                    RID &lastRID,
                                    PageNum &nextPage){
    RC rc = 0;
    PageNum nextPageNum = bHeader->nextBucket;
    //设置nextBucket指针
    nextPage = bHeader->nextBucket;

    struct BucketEntry *bucketEntries = (struct BucketEntry *)((char *)bHeader + indexHeader.bucketEntryOffset);

    //如果在此之后还有一个存储区，请首先在其中搜索是否要删除以下存储区
    if((nextPageNum != NO_MORE_PAGES)){
        bool toDelete = false;
        PF_PageHandle nextBucketPH;
        struct IndexBucketHeader *nextHeader;
        RID last;
        //获取下一个存储区
        PageNum nextNextPage;
        
        //获取页面及其数据内容
//        if((rc = pageFileHandle.GetThisPage(nextPageNum, nextBucketPH)) || (rc = nextBucketPH.GetData((char *&)nextHeader)))
//            return (rc);
        if((rc = pageFileHandle.GetThisPage(nextPageNum, nextBucketPH)) || (rc = nextBucketPH.GetPageData((char *&)nextHeader)))
            return (rc);
        
        //从此存储区递归调用
        rc = deleteFromBucket(nextHeader, rid, toDelete, last, nextNextPage);
        
        //nextHeader的键值数
        int numKeysInNext = nextHeader->keysNum;
        RC rc2 = 0;
        //标记nextPageNum指示的页面被修改并取消锁定
        if((rc2 = pageFileHandle.MarkDirty(nextPageNum)) || (rc2 = pageFileHandle.UnpinPage(nextPageNum)))
            return (rc2);
        
        //如果下一个存储区仅剩一个键，并且给定存储区中有空间，则将lastRID放入给定存储区，然后删除下一个存储区
        if(toDelete && bHeader->keysNum < indexHeader.bucketMaxKeys && numKeysInNext == 1){
            int location = bHeader->emptySlotIndex;
            //获取记录的PageNum和SlotNum
//            if((rc2 = last.GetPageNum(bucketEntries[location].page)) || (rc2 = last.GetSlotNum(bucketEntries[location].slot)))
//                return (rc2);
            bucketEntries[location].page = last.GetPageNum();
            bucketEntries[location].slot = last.GetSlotNum();

            bHeader->emptySlotIndex = bucketEntries[location].nextSlot;
            bucketEntries[location].nextSlot = bHeader->firstSlotIndex;
            bHeader->firstSlotIndex = location;
            
            bHeader->keysNum++;
            numKeysInNext = 0;
        }
        //删除下一个存储区
        if(toDelete && numKeysInNext == 0){
            //销毁页面
            if((rc2 = pageFileHandle.DisposePage(nextPageNum)))
                return (rc2);
            //将此存储区指向已经被删除的存储区指向的存储区
            bHeader->nextBucket = nextNextPage;
        }

        //如果找到该值，则返回
        if(rc == 0)
            return (0);
      }
    
    //否则，在此存储区中搜索
    PageNum ridPage = rid.GetPageNum();
    SlotNum ridSlot = rid.GetSlotNum();
//    if((rc = rid.GetPageNum(ridPage))|| (rc = rid.GetSlotNum(ridSlot)))
//        return (rc);
  
    //搜索整个值
    int prevIndex = BEGINNING_OF_SLOTS;
    int currIndex = bHeader->firstSlotIndex;
    bool found = false;
    //没到结尾继续查找
    while(currIndex != NO_MORE_SLOTS){
        //找到
        if(bucketEntries[currIndex].page == ridPage && bucketEntries[currIndex].slot == ridSlot){
            found = true;
            break;
        }
        prevIndex = currIndex;
        currIndex = bucketEntries[prevIndex].nextSlot;
    }

    //如果找到，从中删除
    if(found){
        //删除更新链表的指针
        if (prevIndex == BEGINNING_OF_SLOTS)
            bHeader->firstSlotIndex = bucketEntries[currIndex].nextSlot;
        else
            bucketEntries[prevIndex].nextSlot = bucketEntries[currIndex].nextSlot;
        bucketEntries[currIndex].nextSlot = bHeader->emptySlotIndex;
        bHeader->emptySlotIndex = currIndex;

        bHeader->keysNum--;
        
        //如果此存储区中有一个或没有的键，将其标记为删除
        if(bHeader->keysNum == 1 || bHeader->keysNum == 0){
            int firstSlot = bHeader->firstSlotIndex;
            RID last(bucketEntries[firstSlot].page, bucketEntries[firstSlot].slot);
            //返回最后一个RID以移至上一个存储区
            lastRID = last;
            deletePage = true;
        }

        return (0);
    }
    
    //如果未找到，则返回IX_INVALIDENTRY
    return (IX_INVALIDENTRY);
}


#pragma mark - 7.辅助函数,友元类中访问的函数
/**
 此方法返回PF_PageHandle和此索引中第一个叶子页的PageNum
*/
RC IX_IndexHandle::getFirstLeafPage(PF_PageHandle &leafPH, PageNum &leafPage){
    RC rc = 0;
    struct IndexNodeHeader *rHeader;
    //检索头部信息
//    if((rc = rootPageHandle.GetData((char *&)rHeader))){
//        return (rc);
//    }
    if((rc = rootPageHandle.GetPageData((char *&)rHeader)))
        return (rc);

    //如果根结点是叶子结点
    if(rHeader->isLeafNode == true){
        leafPH = rootPageHandle;
        leafPage = indexHeader.rootPageNum;
        return (0);
    }

    //根结点非叶子结点 始终向下浏览每个内部节点的第一页以进行向下搜索
    struct IndexInternalNodeHeader *nHeader = (struct IndexInternalNodeHeader *)rHeader;
    PageNum nextPageNum = nHeader->firstPage;
    PF_PageHandle nextPH;
    if(nextPageNum == NO_MORE_PAGES)
        return (IX_EOF);
//    if((rc = pageFileHandle.GetThisPage(nextPageNum, nextPH)) || (rc = nextPH.GetData((char *&)nHeader)))
//        return (rc);
    if((rc = pageFileHandle.GetThisPage(nextPageNum, nextPH)) || (rc = nextPH.GetPageData((char *&)nHeader)))
        return (rc);
    //如果不是叶节点，取消固定并转到其第一个子节点
    while(nHeader->isLeafNode == false){
        PageNum prevPage = nextPageNum;
        nextPageNum = nHeader->firstPage;
        //取消锁定
        if((rc = pageFileHandle.UnpinPage(prevPage)))
            return (rc);
        //获取nextPageNum指向的页面数据
//        if((rc = pageFileHandle.GetThisPage(nextPageNum, nextPH)) || (rc = nextPH.GetData((char *&)nHeader)))
//            return (rc);
        if((rc = pageFileHandle.GetThisPage(nextPageNum, nextPH)) || (rc = nextPH.GetPageData((char *&)nHeader)))
            return (rc);
    }
    leafPage = nextPageNum;
    leafPH = nextPH;

    return (rc);
}

/**
 此方法返回其页面编号leafPage
*/
RC IX_IndexHandle::getRecordPageNum(PF_PageHandle &leafPH, PageNum &leafPage, void *key){
    RC rc = 0;
    struct IndexNodeHeader *rHeader;
    //检索头部信息
//    if((rc = rootPageHandle.GetData((char *&) rHeader)))
//        return (rc);
    if((rc = rootPageHandle.GetPageData((char *&) rHeader)))
        return (rc);
    
    //如果根结点是叶子结点
    if(rHeader->isLeafNode == true){
        leafPH = rootPageHandle;
        leafPage = indexHeader.rootPageNum;
        return (0);
    }

    struct IndexInternalNodeHeader *nHeader = (struct IndexInternalNodeHeader *)rHeader;
    int index = BEGINNING_OF_SLOTS;
    bool isDup = false;
    PageNum nextPageNum;
    PF_PageHandle nextPageHandle;
    
    //获取正确的索引
    if((rc = getNodeInsertIndex((struct IndexNodeHeader *)nHeader, key, index, isDup)))
        return (rc);
    struct NodeEntry *nodeEntries = (struct NodeEntry *)((char *)nHeader + indexHeader.nodeEntryOffset);
    
    //起始位置
    if(index == BEGINNING_OF_SLOTS)
        nextPageNum = nHeader->firstPage;
    else
        nextPageNum = nodeEntries[index].page;
    
    //下一页无效
    if(nextPageNum == NO_MORE_PAGES)
        return (IX_EOF);
  
    //获取当前页和页内数据
//    if((rc = pageFileHandle.GetThisPage(nextPageNum, nextPageHandle)) || (rc = nextPageHandle.GetData((char *&)nHeader)))
//        return (rc);
    if((rc = pageFileHandle.GetThisPage(nextPageNum, nextPageHandle)) || (rc = nextPageHandle.GetPageData((char *&)nHeader)))
        return (rc);
    //不是叶子结点
    while(nHeader->isLeafNode == false){
        //找到给定的键值对应的索引位置
        if((rc = getNodeInsertIndex((struct IndexNodeHeader *)nHeader, key, index, isDup)))
            return (rc);
    
        nodeEntries = (struct NodeEntry *)((char *)nHeader + indexHeader.nodeEntryOffset);
        PageNum prevPage = nextPageNum;
        //是起始位置
        if(index == BEGINNING_OF_SLOTS)
            nextPageNum = nHeader->firstPage;
        else
            nextPageNum = nodeEntries[index].page;
        
        //取消锁定
        if((rc = pageFileHandle.UnpinPage(prevPage)))
            return (rc);
        //获取nextPageNum指向的页面数据
//        if((rc = pageFileHandle.GetThisPage(nextPageNum, nextPageHandle)) || (rc = nextPageHandle.GetData((char *&)nHeader)))
//            return (rc);
        if((rc = pageFileHandle.GetThisPage(nextPageNum, nextPageHandle)) || (rc = nextPageHandle.GetPageData((char *&)nHeader)))
            return (rc);
    }
    
    leafPage = nextPageNum;
    leafPH = nextPageHandle;

    return (rc);
}

/**
 此方法根据给定的属性长度，计算结点中可以容纳的键数
*/
int IX_IndexHandle::getNodeMaxNumKeys(int attrLength){
    int body_size = PF_PAGE_SIZE - sizeof(struct IndexNodeHeader);
    return floor(1.0*body_size / (sizeof(struct NodeEntry) + attrLength));
}

/**
 此方法计算存储区的项数
*/
int IX_IndexHandle::getBucketMaxNumKeys(int attrLength){
    int body_size = PF_PAGE_SIZE - sizeof(struct IndexBucketHeader);
    return floor(1.0*body_size / (sizeof(BucketEntry)));
}


