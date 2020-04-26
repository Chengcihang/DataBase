//
//  rm_record.h
//  DataBaseSystem
//
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//
#ifndef DATABASE_RM_RECORD_H
#define DATABASE_RM_RECORD_H

#include "redbase.h"
#include "rm_rid.h"
#include "rm.h"

/**
 * @brief 记录的抽象
 * 只有data是指向缓冲区，该类的对象存在内存中
 */
class RM_Record {
    //记录长度初始值
    static const int INVALID_RECORD_SIZE = -1;
    friend class RM_FileHandle;
public:
    RM_Record ();                                   // 构造函数
    ~RM_Record();                                   // 希构函数
    RM_Record& operator= (const RM_Record &record); // 等于操作符号重载

    RC GetData(char *&pData) const;                         // 得到记录的数据内容

    RC GetRid (RID &_rid) const;                             // 得到记录的RID

    RC SetRecord (RID rec_rid, char *recData, int size);    // 设置记录
private:
    RID rid;         // 记录的RID
    char * data;     // 记录的数据，从缓冲区中拷贝出来的字段
    int size;        // 一条记录占用的字节数
};


#endif //DATABASE_RM_RECORD_H
