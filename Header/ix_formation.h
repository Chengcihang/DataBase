//
//  IX_Formation.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/14.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef IX_Formation_h
#define IX_Formation_h

#include "ix.h"

#define NO_MORE_PAGES -1
#define NO_MORE_SLOTS -1

#pragma mark - 存储区header
//定义存储区页面的header
struct IndexBucketHeader {
    //存储区中的键值数
    int keysNum;
    //指向存储区中第一个有效插槽的指针
    int firstSlotIndex;
    //指向存储区中第一个空闲插槽的指针
    int emptySlotIndex;
    //指向下一个存储区页面的指针
    PageNum nextBucket;
};

#pragma mark - 结点header
/**
 定义每个结点的Header
 IX_NodeHeader用作所有结点的通用类型转换
 结点是内部结点，就使用IX_InternalNodeHeader
 结点是叶结点，就使用IX_LeafNodeHeader
 这里定义的三个结构体包含的属性类型必须一致 否则类型转换时会出现错误
 */
//通用结点的header
struct IndexNodeHeader {
    //结点持有的有效键值
    int keysNum;

    //指向空闲插槽开始的指针
    int emptySlotIndex;
    //指向以下链接列表开头的指针（有效的指针槽）
    int firstSlotIndex;
     
    PageNum pageNum1;
    PageNum pageNum2;
    
    //指示是否为叶结点
    bool isLeafNode;
    //结点是否包含指针
    bool isEmpty;
};

//内部结点的header
struct IndexInternalNodeHeader {
    //结点持有的有效键值
    int keysNum;

    //指向空闲插槽开始的指针
    int emptySlotIndex;
    //指向以下链接列表开头的指针（有效的指针槽）
    int firstSlotIndex;
    
    //此内部结点下的第一个叶子页
    PageNum firstPage;
    PageNum pageNum2;
    
    //指示是否为叶结点
    bool isLeafNode;
    //结点是否包含指针
    bool isEmpty;
};

//叶子结点的header
struct IndexLeafNodeHeader {
    //结点持有的有效键值
    int keysNum;

    //指向空闲插槽开始的指针
    int emptySlotIndex;
    //指向有效链接列表开头的指针（有效的指针槽）
    int firstSlotIndex;
    
    //下一页
    PageNum nextPage;
    //前一页
    PageNum prePage;
    
    //指示是否为叶结点
    bool isLeafNode;
    //结点是否包含指针
    bool isEmpty;
};

#pragma mark - 项的信息
/**
定义每个项的信息
Entry 项
NodeEntry结点项
BucketEntry存储区项
这里定义的三个结构体包含的属性类型可以不同 因为不涉及到类型转换
*/
//项信息
struct Entry {
    //是否有效
    char isValid;
    //下一个插槽
    int nextSlot;
};

//结点的项信息
struct NodeEntry {
    //是否有效
    char isValid;
    //指向结点中的下一个插槽
    int nextSlot;
    //指向与此键关联的页面
    PageNum page;
    //指向与此条目关联的插槽（仅对叶结点有效）
    SlotNum slot;
};

//存储区项的信息
struct BucketEntry {
    //指向下一个插槽
    int nextSlot;
    //唯一地标识文件中的页面
    PageNum page;
    //唯一标识页面中的记录
    SlotNum slot;
    //不再需要isValid标识位（存储区中的所有条目将是相同值的重复项）
};


#endif /* IX_Formation_h */
