//  rm_manager.h
//  DataBaseSystem
//  记录管理模块的主接口
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//
#ifndef DATABASE_RM_MANAGER_H
#define DATABASE_RM_MANAGER_H
#include "../Header/redbase.h"
#include "../Header/pf.h"
#include "../Header/rm_filehandle.h"

/**
 * 记录管理器
 * 管理的对象是一张张数据表，提供对数据表的创建，销毁，打开和关闭
 * 不同的数据表的模式不同，但是基本组成部分都是记录
 */
class RM_Manager {
public:
    explicit RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);

    // 根据数据表名，得到一个数据表对象fileHandle
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

    RC CloseFile  (RM_FileHandle &fileHandle);
private:
    // 一张数据表读应了文件系统的一个文件，因此需要将两者关联起来
    // 打开一个文件的必要操作
    // 即将RM_FileHandle的pfh指向PF_FileHandle
    RC SetUpFH(RM_FileHandle& fileHandle, PF_FileHandle &fh, RM_FileHeader* header);

    // 关闭一个数据表的必要操作，与SetUpFH对应
    RC CleanUpFH(RM_FileHandle &fileHandle);

    // 页式文件系统模块，创建记录管理模块时需要关联一个页式文件系统模块
    PF_Manager &pfm;
};

#endif //DATABASE_RM_MANAGER_H
