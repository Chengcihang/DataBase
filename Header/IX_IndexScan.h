//
//  IX_IndexScan.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/16.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef IX_IndexScan_h
#define IX_IndexScan_h
#include "IX_IndexHandle.h"


//基于条件的索引项扫描
class IX_IndexScan {
    //检查是否有效的类常量
    static const char UNOCCUPIED = 'u';
    static const char OCCUPIED_NEW = 'n';
    static const char OCCUPIED_DUP = 'r';
    
public:
    //构造函数
    IX_IndexScan();
    //析构函数
    ~IX_IndexScan();

   //打开索引扫描
    RC OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT);
    
    //关闭索引扫描
    RC CloseScan();

    //获取下一个匹配的项，如果没有更多匹配的条目，返回IX_EOF
    RC GetNextEntry(RID &rid);
    
private:
    //私有变量
    
    //指示是否正在使用扫描的指针指向indexHandle的指针，该指针修改扫描将尝试遍历的文件
    bool scanIsOpen;
    IX_IndexHandle *indexHandle;
    
    //是固定存储区还是叶页
    bool isPinnedOfBucket;
    bool isPinnedOfLeaf;
    
    //变量值是否已初始化
    bool isInitialized;
    //是否已扫描到达索引结束
    bool isReachedEndOfIndex;

    //是否找到第一个值
    bool firstValueIsFound;
    //是否找到最后一个值
    bool lastValueIsFound;
    //使用第一个叶子结点
    bool useFirstLeaf;

    //指示扫描是否已开始或结束
    bool scanIsStart;
    bool scanIsEnd;
    
    //比较确定记录是否满足给定的扫描条件
    bool (*comparator) (void *, void*, AttrType, int);
    //比较类型，以及有关要比较的值的信息
    int attributeSize;
    void *value;
    AttrType attributeType;
    CompOp compOperator;
    
    //当前固定扫描正在访问的Leaf和Bucket的PageHandles
    PF_PageHandle currLeafPageHandle;
    PF_PageHandle currBucketPageHandle;
    
    //当前叶子的页码
    PageNum currentLeafPageNum;
    //当前存储区的页码
    PageNum currentBucketPageNum;
    //下一个存储区的页码
    PageNum nextBucketPageNum;
    
    //扫描中的当前RID和下一个RID
    RID currentRID;
    RID nextRID;

    //当前记录的键，以及此后的两个记录的键
    char *currentKey;
    char *nextKey;
    char *nextNextKey;
    
    //扫描的当前叶子和存储区插槽
    int currentLeafSlot;
    int currentBucketSlot;
    
    //扫描的当前叶子和存储区header以及结点项和存储区项和键指针
    struct IndexLeafNodeHeader *leafHeader;
    struct IndexBucketHeader *bucketHeader;
    struct NodeEntry *leafNodeEntries;
    struct BucketEntry *bucketEntries;
    char *leafKeys;
    
    //私有函数
    //开始扫描
    RC startScan(PF_PageHandle &leafPH,
                 PageNum &pageNum);
    
    //获取给定存储区中的第一项
    RC getFirstBucketEntry(PageNum nextBucket,
                           PF_PageHandle &bucketPH);
    
    //获取给定叶子中的第一项
    RC getFirstEntryInLeaf(PF_PageHandle &leafPH);
    
    //获取给定叶子内的合适项
    RC getFitEntryInLeaf(PF_PageHandle &leafPH);
    
    
    //获取索引中的下一项
    RC getNextValueInIndex();

    //设置RID
    RC setRID(bool setCurrent);
};


#endif /* IX_IndexScan_h */
