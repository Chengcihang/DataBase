//
//  ix_indexhandle.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/16.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef IX_IndexHandle_h
#define IX_IndexHandle_h
#include "redbase.h"
#include "rm_rid.h"
#include "indexheader.h"


//索引文件处理接口
class IX_IndexHandle {
    //友元类
    friend class IX_Manager;
    friend class IX_IndexScan;

    //检查节点中的插槽是否有效的类常量
    static const int BEGINNING_OF_SLOTS = -2;
    static const int END_OF_SLOTS = -3;
    static const char UNOCCUPIED = 'u';
    static const char OCCUPIED_NEW = 'n';
    static const char OCCUPIED_DUP = 'r';

public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    //插入新的索引项
    RC InsertEntry(void *pData, const RID &rid);
    //删除索引项
    RC DeleteEntry(void *pData, const RID &rid);
    //将索引文件写会磁盘
    RC ForcePages();
    //把索引对象所在的数据页全部打印显示
    RC PrintIndex();
    
private:
    //私有变量
    struct IndexHeader indexHeader;     // 此索引的header
    PF_PageHandle rootPageHandle;       // 与根结点关联的PF_PageHandle
    PF_FileHandle pageFileHandle;       // 与此索引关联的PF_FileHandle
    bool headerIsModified;              // 指示header是否已修改
    bool handleIsOpen;                  // 是否正在使用
    
    //私有函数
    
    //比较器
    int (*comparator) (void * , void *, int);
    bool (*printer) (void *, int);
    
    //检查标头中给出的值（偏移量，大小等）是否有效的header
    bool indexHeaderIsValid() const;
    
    //创建新的结点和存储区Page
    RC createNewNode(PF_PageHandle &pageHandle,
                     PageNum &pageNum,
                     char *& nodeData,
                     bool isLeafNode);
    
    RC createNewBucket(PageNum &pageNum);

    //拆分给定的结点
    RC divideNode(struct IndexNodeHeader *parentHeader,
                 struct IndexNodeHeader *oldHeader,
                 PageNum oldPage,
                 int index,
                 int &newKeyIndex,
                 PageNum &newPageNum);
    
    //向未满的结点或存储区插入
    RC insertToNode(struct IndexNodeHeader *nHeader,
                           PageNum thisNodeNum,
                           void *pData,
                           const RID &rid);
    
    RC insertToBucket(PageNum pageNum,
                      const RID &rid);


    //找到当前传入值之前的索引
    RC getPreIndex(struct IndexNodeHeader *nHeader,
                   int thisIndex,
                   int &prevIndex);
    
    //找到适当的索引并插入值
    RC getNodeInsertIndex(struct IndexNodeHeader *nHeader,
                           void* pData,
                           int& index,
                           bool& isDuplicate);

    //从内部结点，叶子和存储区中删除
    RC deleteFromNode(struct IndexNodeHeader *nHeader,
                      void *pData,
                      const RID &rid,
                      bool &toDelete);
    
    RC deleteFromBucket(struct IndexBucketHeader *bHeader,
                        const RID &rid,
                        bool &deletePage,
                        RID &lastRID,
                        PageNum &nextPage);
    
    RC deleteFromLeafNode(struct IndexLeafNodeHeader *nHeader,
                          void *pData,
                          const RID &rid,
                          bool &toDelete);

    
    //部分辅助函数 帮助友元类进行处理
    //给定属性长度，计算最大条目数,用于存储区和结点
    static int getNodeMaxNumKeys(int attrLength);
    static int getBucketMaxNumKeys(int attrLength);

    //返回leafPH中的第一个叶子页面/返回指定键值的页面编号leafPage
    RC getFirstLeafPage(PF_PageHandle &leafPH,
                        PageNum &leafPage);
    
    RC getRecordPageNum(PF_PageHandle &leafPH,
                        PageNum &leafPage,
                        void * key);
};



#endif /* IX_IndexHandle_h */
