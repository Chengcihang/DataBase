//
//  SM_Catalog.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/28.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef SM_Catalog_h
#define SM_Catalog_h

#include "../IX/ix.h"

//定义关系的目录项
typedef struct RelCatEntry {
    //关系名
    char relName[MAXNAME + 1];
    //元组长度
    int tupleLength;
    //属性数量
    int attrCount;
    //索引数量
    int indexCount;
    //当前索引标号
    int indexCurrNum;
    //元组数
    int numTuples;
    //统计信息是否设置标识位
    bool statsInitialized;
} RelCatEntry;


//定义属性的目录项
typedef struct AttrCatEntry{
    //关系名
    char relName[MAXNAME + 1];
    //属性名
    char attrName[MAXNAME +1];
    //偏移量
    int offset;
    //属性类型
    AttrType attrType;
    //属性长度
    int attrLength;
    //索引标号
    int indexNo;
    //属性数量
    int attrNum;
    //不同数量
    int numDistinct;
    //最大值
    float maxValue;
    //最小值
    float minValue;
} AttrCatEntry;


//定义属性的信息
typedef struct Attr{
    //偏移量
    int offset;
    //类型
    int type;
    //长度
    int length;
    //索引标号
    int indexNo;
    //索引文件处理器
    IX_IndexHandle ih;
    bool (*recInsert) (char *, std::string, int);
    //不同数量
    int numDistinct;
    //最大值
    float maxValue;
    //最小值
    float minValue;
} Attr;


#endif /* SM_Catalog_h */
