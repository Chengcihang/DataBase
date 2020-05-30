//
//  SM_Scan.cpp
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/27.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#include "../RM/rm.h"
#include "sm_catalog.h"
#include "sm_scan.h"

using namespace std;


//构造函数
SM_Scan::SM_Scan() {
    scanIsValid = false;
}


//析构函数，如果当前有效则关闭
SM_Scan::~SM_Scan() {
    if(scanIsValid == true){
        fileScan.CloseScan();
    }
    scanIsValid = false;
}


//创建与指定关系相关的属性的扫描
RC SM_Scan::OpenScan(RM_FileHandle &fh,
                     char *relName) {
    RC rc = 0;
    scanIsValid = true;
    //初始化一个文件扫描器
    if((rc = fileScan.OpenScan(fh, STRING, MAXNAME+1, 0, EQ_OP, relName)))
        return (rc);

    return (0);
}


//返回与之关联的下一个属性
RC SM_Scan::GetNextAttr(RM_Record &attrRec,
                                AttrCatEntry *&entry){
    RC rc = 0;
    //检索下一条记录
    if((rc = fileScan.GetNextRec(attrRec)))
        return (rc);
    //获取数据
    if((rc = attrRec.GetData((char *&)entry)))
        return (rc);
    return (0);
}


//关闭与此相关的扫描
RC SM_Scan::CloseScan(){
    RC rc = 0;
    //关闭扫描
    if((rc = fileScan.CloseScan()))
        return (rc);
    //修改标识位
    scanIsValid = false;
    return (0);
}
