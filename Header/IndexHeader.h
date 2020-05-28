//
//  IndexHeader.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/16.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef IndexHeader_h
#define IndexHeader_h
#include "redbase.h"
#include "pf.h"


//索引的头部
struct IndexHeader {
    //属性类型和长度
    AttrType attributeType;
    int attributeSize;

    //结点和存储区中项的偏移量
    int nodeEntryOffset;
    int bucketEntryOffset;

    //结点中键列表的偏移量
    int nodeKeysOffset;
    
    //结点和存储区中最大条目数
    int nodeMaxKeys;
    int bucketMaxKeys;

    //关联根页的PageNum
    PageNum rootPageNum;
};


#endif /* IndexHeader_h */
