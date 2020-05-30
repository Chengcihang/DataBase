//
//  rm_record.h
//  DataBaseSystem
//
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//
#include <unistd.h>
#include <sys/types.h>
#include <cstring>
#include "../PF/pf.h"
#include "rm_record.h"

/**
 * @brief 构造函数
 * @param 无
 * @return 无
 */
RM_Record::RM_Record(){
    size = INVALID_RECORD_SIZE;
    data = NULL;
    rid = RID();
}

/**
 * @brief 析构函数
 * @param 无
 * @return 无
 */
RM_Record::~RM_Record(){
    if(!this->data)
        delete [] data;
}

/**
 * @brief =操作符重载，拷贝一条记录
 * @param RM_Record
 * @return (*this)
 */
RM_Record& RM_Record::operator= (const RM_Record &record){
    if (this != &record){
        if(!this->data)
            delete [] data;
        this->size = record.size;
        this->data = new char[size];
        memcpy(this->data, record.data, record.size);
        this->rid = record.rid;
  }
  return (*this);
}

/**
 * @brief 得到记录数据
 * @param char *&pData 指向记录的指针
 * @return 操作状态
 *          RM_INVALIDRECORD 非法的记录
 *          OK_RC            成功
 */
RC RM_Record::GetData(char *&pData) const {
    if(data == NULL || size == INVALID_RECORD_SIZE)
        return (RM_INVALIDRECORD);
    // 取得数据指针
    pData = data;
    return OK_RC;
}

/**
 * @brief 得到记录的RID
 * @param RID &rid 指向记录的RID
 * @return 操作状态
 *          rc       失败
 *          OK_RC    成功
 */
RC RM_Record::GetRid (RID &_rid) const {
    RC rc;
    if((rc = (this->rid).isValidRID()))
        return rc;
    _rid = this->rid;
    return OK_RC;
}


/**
 * @brief 设置Record
 * @param RID rec_rid   RID
 * @param char *recData 记录数据
 * @param int rec_size  记录长度
 * @return 操作状态
 *          rc       失败
 *          OK_RC    成功
 */
RC RM_Record::SetRecord(RID rec_rid, char *recData, int rec_size){
    RC rc;
    if((rc = rec_rid.isValidRID()))
        return RM_INVALIDRID;
    if(rec_size <= 0 )
        return RM_BADRECORDSIZE;
    if(recData == NULL)
        return RM_INVALIDRECORD;
    if (data != NULL)
        delete [] data;

    rid = rec_rid;
    size = rec_size;
    data = new char[rec_size];
    memcpy(data, recData, size);
    return OK_RC;
}


