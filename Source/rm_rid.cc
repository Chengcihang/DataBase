//
//  rm_rid.cc
//  DataBaseSystem
//
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//
#include "../Header//rm.h"
#include "../Header/rm_rid.h"

RID::RID(){
    // 初始不可用的成员变量值
    pageNum = INVALID_PAGE;
    slotNum = INVALID_SLOT;
}

RID::RID(PageNum pageNum, SlotNum slotNum) {
    this->pageNum = pageNum;
    this->slotNum = slotNum;
}

RID::~RID()=default;

/*
 * 复制操作
 */
RID& RID::operator= (const RID &rid){
    if (this != &rid){
        this->pageNum = rid.pageNum;
        this->slotNum = rid.slotNum;
    }
    return (*this);
}

Boolean RID::operator== (const RID &rid) const{
    // 页号和槽号都相等时返回真
    return (this->pageNum == rid.pageNum && this->slotNum == rid.slotNum);
}

//  返回页号
PageNum RID::GetPageNum() const {
    return this->pageNum;
}

// 返回槽号
SlotNum RID::GetSlotNum() const {
    return this->slotNum;
}

// 检查此RID是否有效
RC RID::isValidRID() const{
    if(pageNum > 0 && slotNum >= 0)
        return OK_RC;
    else
        return RM_INVALIDRID;
}

RC RID::SetPageNum(PageNum _pageNum) {
    if(_pageNum > 0 || _pageNum == INVALID_PAGE){
        this->pageNum = _pageNum;
        return OK_RC;
    }
    return RM_INVALIDRID;
}

RC RID::SetSlotNum(SlotNum _slotNum) {
    if(_slotNum >= 0 || _slotNum == INVALID_SLOT){
        this->slotNum = _slotNum;
        return OK_RC;
    }
    return RM_INVALIDRID;
}