//
//  SM_Scan.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/27.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef SM_Scan_h
#define SM_Scan_h

#include "sm.h"
#include "../RM/rm_record.h"
#include "../RM/rm_filehandle.h"

//属性迭代器
class SM_Scan {
    friend class SM_Manager;

public:
    //构造函数
    SM_Scan ();
    
    //析构函数
    ~SM_Scan ();
    
    //打开扫描
    RC OpenScan(RM_FileHandle &fh,
                    char *relName);
    
    //获取下一个属性
    RC GetNextAttr(RM_Record &attrRec,
                   AttrCatEntry *&entry);
    
    //关闭迭代器
    RC CloseScan();

private:
    //是否为有效迭代器的标识位
    bool scanIsValid;
    
    //文件扫描器
    RM_FileScan fileScan;
};


#endif /* SM_AttrIterator_h */
