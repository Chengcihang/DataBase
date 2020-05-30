//
//  ix_manager.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/16.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef IX_Manager_h
#define IX_Manager_h
#include "../PF/pf.h"
#include "ix_indexhandle.h"


//提供索引文件管理
class IX_Manager {
    //检查是否有效的类常量
    static const char UNOCCUPIED = 'u';
public:
    //构造函数
    IX_Manager(PF_Manager &pfm);
    //析构函数
    ~IX_Manager();

    //创建一个新索引
    RC CreateIndex(const char *fileName,
                   int indexNo,
                   AttrType attrType,
                   int attrLength);
    //销毁并建立索引
    RC DestroyIndex(const char *fileName,
                    int indexNo);

    //打开索引
    RC OpenIndex(const char *fileName,
                 int indexNo,
                 IX_IndexHandle &indexHandle);

    //关闭索引
    RC CloseIndex(IX_IndexHandle &indexHandle);

private:
    //私有变量
    //与该索引关联的PF_Manager
    PF_Manager &pfManager;
    
    //检查给定的索引参数（attrtype和length）是否构成有效索引
    bool indexIsValid(AttrType attrType,
                      int attrLength);

    //根据文件名和索引号创建索引文件名，并将其作为字符串返回给indexname
    RC getIndexFileName(const char *fileName,
                        int indexNo,
                        std::string &indexname);
    
    //打开索引时设置Index Handle内部变量
    RC initIndexHandle(IX_IndexHandle &indexHandle,
                        PF_FileHandle &fileHandle,
                        struct IndexHeader *indexHeader);
    
    //关闭索引时修改Index Handle内部变量
    RC closeIndexHandle(IX_IndexHandle &indexHandle);
};


#endif /* IX_Manager_h */
