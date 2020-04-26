//
//  rm_rid.h
//  DataBaseSystem
//
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef RM_RID_H
#define RM_RID_H

// rid由页号和槽号组成，标识了一条记录的位置
// 同时也是一条记录的唯一标识

#include "redbase.h"
#include "../Header/pf.h"

// PageNum类型保持和pf.h中的一致
// 槽号，表示记录在页中的第几个位置
//typedef int SlotNum;


// rid由页号和槽号组成，标识了一条记录的位置，同时也是一条记录的唯一标识
// RID类
// RID对象并不存储在内存或者磁盘上
// 其实不需要是个类，但是接口里有要求
class RID {
    // 初始化一个RID对象时用到
    // 无效的页号
    static const PageNum INVALID_PAGE = -1;
    // 无效的槽号
    static const SlotNum INVALID_SLOT = -1;
public:
    RID();                                          // 构造函数
    RID(PageNum pageNum, SlotNum slotNum);          // 带参数构造函数
    ~RID();
    RID& operator= (const RID &rid);                // 复制操作符重载
    Boolean operator== (const RID &rid) const;      // 等于操作符重载

    RC SetPageNum(PageNum _pageNum);                 // 设置页号
    RC SetSlotNum(SlotNum _slotNum);                 // 设置槽号

    PageNum GetPageNum () const;                    // 返回页号
    SlotNum GetSlotNum () const;                    // 返回槽号

    RC isValidRID() const;                          // 判断是否为有效的RID,若无效，返回RM_INVALIDRID
                                                    // 否则返回OK_RC,表示有效

private:
    PageNum pageNum;
    SlotNum slotNum;
};


#endif
