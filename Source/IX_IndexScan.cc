//
//  IX_IndexScan.cpp
//  MicroDBMS
//
//  Created by 全俊源 on 2020/3/23.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#include <unistd.h>
#include <sys/types.h>
#include "../Header/pf.h"
#include "../Header/IX_Formation.h"
#include "../Header/IX_Error.h"
#include <cstdio>
#include "../Header/IX_IndexScan.h"

#pragma mark - comparison functions
/**
 下列函数是比较函数，如果两个对象相等，则返回0；
 如果第一个值较小，则返回<0；
 如果第二个值较小，则返回> 0
 采用相同的属性类型和属性长度，这确定了比较值的基础
 */
bool isEqual(void * value1, void * value2, AttrType attrtype, int attrLength){
    switch(attrtype){
        case FLOAT:
            return (*(float *)value1 == *(float*)value2);
        case INT:
            return (*(int *)value1 == *(int *)value2) ;
        default:
            return (strncmp((char *) value1, (char *) value2, attrLength) == 0);
    }
}

bool isLess(void * value1, void * value2, AttrType attrtype, int attrLength){
    switch(attrtype){
        case FLOAT:
            return (*(float *)value1 < *(float*)value2);
        case INT:
            return (*(int *)value1 < *(int *)value2) ;
        default:
            return (strncmp((char *) value1, (char *) value2, attrLength) < 0);
    }
}

bool isGreater(void * value1, void * value2, AttrType attrtype, int attrLength){
    switch(attrtype){
        case FLOAT:
            return (*(float *)value1 > *(float*)value2);
        case INT:
            return (*(int *)value1 > *(int *)value2) ;
        default:
            return (strncmp((char *) value1, (char *) value2, attrLength) > 0);
    }
}

bool isLessOrEqual(void * value1, void * value2, AttrType attrtype, int attrLength){
    switch(attrtype){
        case FLOAT:
            return (*(float *)value1 <= *(float*)value2);
        case INT:
            return (*(int *)value1 <= *(int *)value2) ;
        default:
            return (strncmp((char *) value1, (char *) value2, attrLength) <= 0);
    }
}

bool isGreaterOrEqual(void * value1, void * value2, AttrType attrtype, int attrLength){
    switch(attrtype){
        case FLOAT:
            return (*(float *)value1 >= *(float*)value2);
        case INT:
            return (*(int *)value1 >= *(int *)value2) ;
        default:
            return (strncmp((char *) value1, (char *) value2, attrLength) >= 0);
    }
}

bool isNotEqual(void * value1, void * value2, AttrType attrtype, int attrLength){
    switch(attrtype){
        case FLOAT:
            return (*(float *)value1 != *(float*)value2);
        case INT:
            return (*(int *)value1 != *(int *)value2) ;
        default:
            return (strncmp((char *) value1, (char *) value2, attrLength) != 0);
    }
}


#pragma mark - public functions
/**
 公有函数
 IX_IndexScan();
 
 ~IX_IndexScan();
 
 RC OpenScan(const IX_IndexHandle &indexHandle,
            CompOp compOperator,
            void *value,
            ClientHint  pinHint = NO_HINT);
 
 RC CloseScan();
 
 RC GetNextEntry(RID &rid);
 
*/

#pragma mark - 1.构造函数&析构函数
//构造函数
IX_IndexScan::IX_IndexScan(){
    //初始化赋值
    scanIsOpen = false;
    value = NULL;
    isInitialized = false;
    
    isPinnedOfBucket = false;
    isPinnedOfLeaf = false;
    
    scanIsEnd = true;
    scanIsStart = false;
    
    isReachedEndOfIndex = true;
    
    attributeSize = 0;
    attributeType = INT;

    firstValueIsFound = false;
    lastValueIsFound = false;
    
    useFirstLeaf = false;
}

//析构函数
IX_IndexScan::~IX_IndexScan(){
    //扫描未结束且存储区页被锁定，则取消锁定
    if(scanIsEnd == false && isPinnedOfBucket == true)
        indexHandle->pageFileHandle.UnpinPage(currentBucketPageNum);
    //扫描未结束，叶子结点区页被锁定且当前叶子结点的PageNum不是指向indexHandle里包含的根结点页，则取消锁定
    if(scanIsEnd == false && isPinnedOfLeaf == true &&(currentLeafPageNum != (indexHandle->indexHeader).rootPageNum))
        indexHandle->pageFileHandle.UnpinPage(currentLeafPageNum);
    ////变量值已初始化
    if(isInitialized == true){
        free(value);
        isInitialized = false;
    }
}


#pragma mark - 2.打开/关闭扫描
/**
 此方法初始化一个 indexHandle 指代的索引的条件扫描器，其返回所有满足条件的记录的标识符
 参数indexHandle为打开的索引文件处理器
 参数compOp为比较器
 参数value为索引的键值
 参数pinHint为ClientHint
 */
RC IX_IndexScan::OpenScan (const IX_IndexHandle &indexHandle,
                           CompOp compOp,
                           void *value,
                           ClientHint pinHint){
    RC rc = 0;

    //保证此函数调用时扫描器尚未打开，并禁止NE_OP比较器
    if(scanIsOpen == true || compOp == NE_OP)
        return (IX_INVALIDSCAN);

    //如果打开的indexHanlde索引文件处理器有效，更新扫描器indexHandle属性指向输入参数中的indexHanlde
    if(indexHandle.indexHeaderIsValid())
        this->indexHandle = const_cast<IX_IndexHandle*>(&indexHandle);
    else
        return (IX_INVALIDSCAN);

    this->value = NULL;
    useFirstLeaf = true;
    
    //设置比较器值
    this->compOperator = compOp;
    //分支处理 调用不同的比较函数
    switch(compOp){
        case EQ_OP :
            comparator = &isEqual;
            useFirstLeaf = false;
            break;
        case LT_OP :
            comparator = &isLess;
            break;
        case GT_OP :
            comparator = &isGreater;
            useFirstLeaf = false;
            break;
        case LE_OP :
            comparator = &isLessOrEqual;
            break;
        case GE_OP :
            comparator = &isGreaterOrEqual;
            useFirstLeaf = false;
            break;
        case NO_OP :
            comparator = NULL; break;
        default:
            return (IX_INVALIDSCAN);
    }

    //设置属性的长度和类型
    this->attributeSize = ((this->indexHandle)->indexHeader).attributeSize;
    this->attributeType = (indexHandle.indexHeader).attributeType;
    
    //如比较器值为NO_OP 不进行比较
    if(compOp != NO_OP){
        //设置value 并修改初始化标志位为真
        this->value = (void *) malloc(attributeSize);
        memcpy(this->value, value, attributeSize);
        isInitialized = true;
    }

    //设置扫描器的所有相关的标识位
    //扫描器已打开
    scanIsOpen = true;
    //扫描未开始
    scanIsStart = false;
    //扫描未到末尾
    scanIsEnd = false;
    //叶子结点未锁定
    isPinnedOfLeaf = false;
    //已扫描到达索引结束
    isReachedEndOfIndex = false;
    //未找到第一个和最后一个值
    firstValueIsFound = false;
    lastValueIsFound = false;

    return (rc);
}

/**
 此方法终止索引扫描
 参数无
 */
RC IX_IndexScan::CloseScan () {
    RC rc = 0;
    //判断扫描指针是否是打开状态
    if(!scanIsOpen)
        return (IX_INVALIDSCAN);
    
    //如果扫描已结束 并且当前存储区页面被锁定 则取消锁定
    if(!scanIsEnd && isPinnedOfBucket)
        indexHandle->pageFileHandle.UnpinPage(currentBucketPageNum);
    
    //如果扫描未结束 当前叶子结点页面被锁定 并且currentLeafPageNum指向的页面不是根页面 则取消锁定
    if(!scanIsEnd && isPinnedOfLeaf && (currentLeafPageNum != (indexHandle->indexHeader).rootPageNum))
        indexHandle->pageFileHandle.UnpinPage(currentLeafPageNum);
    
    //如果已经被初始化
    if(isInitialized){
        free(value);
        isInitialized = false;
    }
    
    //更改标识位 设置扫描指针关闭且扫描未开始
    scanIsOpen = false;
    scanIsStart = false;

    return (rc);
}


#pragma mark - 2.获取下一条记录的RID
/**
 此方法把参数 rid 设置为索引扫描中下一条记录的标识符
 参数rid为给定的RID
 */
RC IX_IndexScan::GetNextEntry (RID &rid) {
    RC rc = 0;
    //如果扫描结束，返回IX_EOF
    if(scanIsEnd && scanIsOpen)
        return (IX_EOF);
    //如果最后一个值已经找到页返回IX_EOF
    if(lastValueIsFound)
        return (IX_EOF);

    //如果扫描器并未打开，则返回IX_INVALIDSCAN
    if(scanIsEnd || !scanIsOpen)
        return (IX_INVALIDSCAN);

    //指示是否找到下一个值
    bool hasFoundFlag = true;
    //如果还没找到
    while(hasFoundFlag) {
        //扫描的第一次迭代
        if(!scanIsEnd && scanIsOpen && !scanIsStart){
            //开始扫描，检索第一个项
            if((rc = startScan(currLeafPageHandle, currentLeafPageNum)))
                return (rc);
            //保存当前键值
            currentKey = nextNextKey;
            //设置现在开始扫描的标识位为真
            scanIsStart = true;
            //设置当前的RID
            setRID(true);
            //获取索引中的下一项 如果不存在，则标记已到达扫描结束
            if((IX_EOF == getNextValueInIndex()))
                isReachedEndOfIndex = true;
        }
        else{
            //否则，通过更新当前值来继续扫描
            currentKey = nextKey;
            currentRID = nextRID;
        }
        //设置nextRID并更新nextKey
        setRID(false);
        nextKey = nextNextKey;

        //获取索引中的下一项 如果不存在，则标记已到达扫描结束
        if((IX_EOF == getNextValueInIndex())){
            isReachedEndOfIndex = true;
        }

        PageNum thisRIDPageNum;
        //检查当前RID是否为无效值
//        if((rc = currentRID.GetPageNum(thisRIDPageNum)))
//            return (rc);
        thisRIDPageNum = currentRID.GetPageNum();
        //thisRIDPageNum无效 则表示扫描已结束，返回IX_EOF
        if(thisRIDPageNum == -1){
            scanIsEnd = true;
            return (IX_EOF);
        }

        //不进行比较的情况下，如果找到下一个期望值，则将rid设置为currentRID，并修改标识位
        if(compOperator == NO_OP){
            rid = currentRID;
            hasFoundFlag = false;
            firstValueIsFound = true;
        }
        //比较当前键值currentKey和value属性值的大小
        else if((comparator((void *)currentKey, value, attributeType, attributeSize))){
            rid = currentRID;
            hasFoundFlag = false;
            firstValueIsFound = true;
        }
        else if(firstValueIsFound){
            lastValueIsFound = true;
            return (IX_EOF);
        }
    }
    
    //获取currentRID的SlotNum
    SlotNum thisRIDpage;
    thisRIDpage = currentRID.GetSlotNum();
//    currentRID.GetSlotNum(thisRIDpage);
    return (rc);
}



#pragma mark - private functions
/**
 私有函数
 RC startScan(PF_PageHandle &leafPH,
            PageNum &pageNum);
 
 RC getFirstBucketEntry(PageNum nextBucket,
                  PF_PageHandle &bucketPH);
 
 RC getFirstEntryInLeaf(PF_PageHandle &leafPH);
 
 RC getFitEntryInLeaf(PF_PageHandle &leafPH);
 
 RC getNextValueInIndex();
 
 RC setRID(bool setCurrent);
 
*/

#pragma mark - 1.开始扫描
/**
 此方法开始扫描
 参数leafPH是打开的PF_PageHandle
 参数pageNum是返回的PageNum
*/
RC IX_IndexScan::startScan(PF_PageHandle &leafPH, PageNum &pageNum){
    RC rc = 0;
    //使用第一个叶子结点
    if(useFirstLeaf){
        //返回leafPH和此索引中第一个叶子页的PageNum
        if((rc = indexHandle->getFirstLeafPage(leafPH, pageNum)))
            return (rc);
        //检索打开的leafPH中的第一项
        if((rc = getFirstEntryInLeaf(currLeafPageHandle))){
            if(rc == IX_EOF){
                //设置扫描结束
                scanIsEnd = true;
            }
            return (rc);
        }
    }
    else{
        //返回给定键值的页面编号leafPage
        if((rc = indexHandle->getRecordPageNum(leafPH, pageNum, value)))
            return (rc);
        //获取给定叶子内的合适项
        if((rc = getFitEntryInLeaf(currLeafPageHandle))){
            if(rc == IX_EOF){
                scanIsEnd = true;
            }
            return (rc);
        }
    }
    return (rc);
}


#pragma mark - 2.获取给定存储区/叶子中的第一项/合适项
/**
 此方法在给定存储区PageNum的情况下检索打开的bucketPH的中的第一项
 参数nextBucket是给定的PageNum
 参数bucketPH是打开的bucketPH
 */
RC IX_IndexScan::getFirstBucketEntry(PageNum nextBucket,
                                     PF_PageHandle &bucketPH){
    RC rc = 0;
    //获取页面内容并锁定页面
    if((rc = (indexHandle->pageFileHandle).GetThisPage(nextBucket, currBucketPageHandle)))
        return (rc);
    isPinnedOfBucket = true;
    
    //获取存储区内容
//    if((rc = bucketPH.GetData((char *&) bucketHeader)))
//        return (rc);
    if((rc = bucketPH.GetPageData((char *&) bucketHeader)))
        return (rc);
    
    //存储区项链表
    bucketEntries = (struct BucketEntry *) ((char *)bucketHeader + (indexHandle->indexHeader).bucketEntryOffset);
    //将当前扫描设置为存储区中的第一个插槽
    currentBucketSlot = bucketHeader->firstSlotIndex;

    return (0);
}

/**
 此方法检索打开的leafPH中的第一项
 参数leafPH是打开的PF_PageHandle
 */
RC IX_IndexScan::getFirstEntryInLeaf(PF_PageHandle &leafPH){
    RC rc = 0;
    //锁定叶子页
    isPinnedOfLeaf = true;
    //获取数据
//    if((rc = leafPH.GetData((char *&) leafHeader)))
//        return (rc);
    if((rc = leafPH.GetPageData((char *&) leafHeader)))
        return (rc);

    //叶子中没有键值，返回IX_EOF
    if(leafHeader->keysNum == 0)
        return (IX_EOF);

    //叶子结点链表和键值链表
    leafNodeEntries = (struct NodeEntry *)((char *)leafHeader + (indexHandle->indexHeader).nodeEntryOffset);
    leafKeys = (char *)leafHeader + (indexHandle->indexHeader).nodeKeysOffset;

    //设置当前叶子插槽为leafHeader的第一个插槽索引
    currentLeafSlot = leafHeader->firstSlotIndex;
    //如果currentLeafSlot不是结尾
    if((currentLeafSlot != NO_MORE_SLOTS)){
        //更新nextNextKey
        nextNextKey = leafKeys + attributeSize*currentLeafSlot;
    }
    else
        return (IX_INVALIDSCAN);
    
    //如果是重复值，则进入存储区以检索第一项
    if(leafNodeEntries[currentLeafSlot].isValid == OCCUPIED_DUP){
        //获取当前存储区的页码
        currentBucketPageNum = leafNodeEntries[currentLeafSlot].page;
        //给定PageNum检索打开的bucketPH的中的第一项
        if((rc = getFirstBucketEntry(currentBucketPageNum, currBucketPageHandle)))
            return (rc);
    }
    return (0);
}

/**
 此方法检索打开的leafPH中的合适项
 参数leafPH是打开的PF_PageHandle
*/
RC IX_IndexScan::getFitEntryInLeaf(PF_PageHandle &leafPH){
    RC rc = 0;
    //叶子页锁定
    isPinnedOfLeaf = true;
    //获取leafHeader指向的页面内容
//    if((rc = leafPH.GetData((char *&) leafHeader)))
//        return (rc);
    if((rc = leafPH.GetPageData((char *&) leafHeader)))
        return (rc);
    //如果叶子结点内没有索引键值
    if(leafHeader->keysNum == 0)
        return (IX_EOF);

    //叶子结点链表和和键值链表
    leafNodeEntries = (struct NodeEntry *)((char *)leafHeader + (indexHandle->indexHeader).nodeEntryOffset);
    leafKeys = (char *)leafHeader + (indexHandle->indexHeader).nodeKeysOffset;
    
    
    int index = 0;
    bool isDuplicate = false;
    //找到给定的键值对应的索引位置
    if((rc = indexHandle->getNodeInsertIndex((struct IndexNodeHeader *)leafHeader, value, index, isDuplicate)))
        return (rc);

    //更新当前叶子插槽为刚才调用函数找到的索引位置
    currentLeafSlot = index;
    
    //如果索引有效，更新nextNextKey
    if((currentLeafSlot != NO_MORE_SLOTS))
        nextNextKey = leafKeys + attributeSize* currentLeafSlot;
    else
        return (IX_INVALIDSCAN);

    //如果是重复值，则检索存储区检索第一项
    if(leafNodeEntries[currentLeafSlot].isValid == OCCUPIED_DUP){
        currentBucketPageNum = leafNodeEntries[currentLeafSlot].page;
        if((rc = getFirstBucketEntry(currentBucketPageNum, currBucketPageHandle)))
            return (rc);
    }
    return (0);
}


#pragma mark - 3.获取索引中的下一项
/**
 此方法是获取索引中的下一项，更新与此扫描关联的相关私有变量
 参数无
 */
RC IX_IndexScan::getNextValueInIndex(){
    RC rc = 0;
    //如果存储区已固定，则在此存储区中进行搜索
    if(isPinnedOfBucket){
        int preSlot = currentBucketSlot;
        //更新当前存储区插槽
        currentBucketSlot = bucketEntries[preSlot].nextSlot;
        //找到下一个存储区插槽，并且其是有效的，则停止搜索
        if(currentBucketSlot != NO_MORE_SLOTS){
        
            return (0);
        }
        
        //未找到，取消对此存储区的锁定
        PageNum nextBucket = bucketHeader->nextBucket;
        if((rc = (indexHandle->pageFileHandle).UnpinPage(currentBucketPageNum) ))
            return (rc);
        //更新锁定存储区的标识位
        isPinnedOfBucket = false;

        //如果这是有效的存储区，则将其打开并获取第一项
        if(nextBucket != NO_MORE_PAGES){
            //在给定存储区nextBucket下检索打开的currBucketPageHandle的中的第一项
            if((rc = getFirstBucketEntry(nextBucket, currBucketPageHandle) ))
                return (rc);
            currentBucketPageNum = nextBucket;
            return (0);
        }
    }
    //如果存储区未固定，处理叶子
    int preLeafSlot = currentLeafSlot;
    //更新下一个叶子插槽
    currentLeafSlot = leafNodeEntries[preLeafSlot].nextSlot;

    //叶子插槽有效且叶子插槽包含重复项，打开与其关联的存储区，并更新下下个记录的键值
    if(currentLeafSlot != NO_MORE_SLOTS && leafNodeEntries[currentLeafSlot].isValid == OCCUPIED_DUP){
        nextNextKey = leafKeys + currentLeafSlot * attributeSize;
        currentBucketPageNum = leafNodeEntries[currentLeafSlot].page;
    
        if((rc = getFirstBucketEntry(currentBucketPageNum, currBucketPageHandle) ))
            return (rc);
        return (0);
    }
    //叶子插槽有效，且不包含重复项，更新下下个记录的键值
    if(currentLeafSlot != NO_MORE_SLOTS && leafNodeEntries[currentLeafSlot].isValid == OCCUPIED_NEW){
        nextNextKey = leafKeys + currentLeafSlot * attributeSize;
        return (0);
    }

    //除以上两种情况的处理
    //获取下一页
    PageNum nextLeafPage = leafHeader->nextPage;

    //如果不是根页面，取消对页面的锁定，并更新标识位isPinnedOfLeaf
    if((currentLeafPageNum != (indexHandle->indexHeader).rootPageNum)){
        if((rc = (indexHandle->pageFileHandle).UnpinPage(currentLeafPageNum))){
            return (rc);
        }
    }
    isPinnedOfLeaf = false;

    //如果下一页是有效页，则在此叶子页中检索第一项
    if(nextLeafPage != NO_MORE_PAGES){
        currentLeafPageNum = nextLeafPage;
        //获取页面
        if((rc = (indexHandle->pageFileHandle).GetThisPage(currentLeafPageNum, currLeafPageHandle)))
            return (rc);
        //检索打开的leafPH中的第一项
        if((rc = getFirstEntryInLeaf(currLeafPageHandle) ))
            return (rc);
        return (0);
    }

    //不再有元素的情况
    return (IX_EOF);
}


#pragma mark - 4.设置RID
/**
 此方法是设置私有变量RID之一
 参数setCurrent为用于判断的标识位
 如果setCurrent为true，则设置currRID
 如果setCurrent为false，则设置nextRID
 */
RC IX_IndexScan::setRID(bool setCurrent){
    //如果已到达扫描结束，则将nextRID设置为无效值
    if(isReachedEndOfIndex && !setCurrent){
        RID rid1(-1,-1);
        nextRID = rid1;
        return (0);
    }

    //setCurrent为真 则设置currRID
    if(setCurrent){
        //如果存储区已固定，使用存储区插槽设置RID
        if(isPinnedOfBucket){
            RID rid(bucketEntries[currentBucketSlot].page, bucketEntries[currentBucketSlot].slot);
            currentRID = rid;
        }
        //否则，使用叶子结点槽设置RID
        else if(isPinnedOfLeaf){
            RID rid1(leafNodeEntries[currentLeafSlot].page, leafNodeEntries[currentLeafSlot].slot);
            currentRID = rid1;
        }
    }
    //setCurrent为假 则设置nextRID
    else{
        //如果存储区已固定，使用存储区插槽设置RID
        if(isPinnedOfBucket){
            RID rid(bucketEntries[currentBucketSlot].page, bucketEntries[currentBucketSlot].slot);
            nextRID = rid;
        }
        //否则，使用叶子结点槽设置RID
        else if(isPinnedOfLeaf){
            RID rid1(leafNodeEntries[currentLeafSlot].page, leafNodeEntries[currentLeafSlot].slot);
            nextRID = rid1;
        }
    }
    return (0);
}







