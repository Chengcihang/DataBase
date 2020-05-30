//
//  SM_Error.cpp
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/28.
//  Copyright © 2020 社区风险项目. All rights reserved.
//


#include <cstdio>
#include <cerrno>
#include <iostream>
#include <cstring>
#include "sm.h"
#include "../redbase.h"
#include "sm_printerror.h"
#include "../PF/pf.h"

using namespace std;

//错误表
static char *SM_WarnMsg[] = {
    (char*)"cannot close db",
    (char*)"bad relation name",
    (char*)"bad relation specification",
    (char*)"bad attribute",
    (char*)"invalid attribute",
    (char*)"attribute indexed already",
    (char*)"attribute has no index",
    (char*)"invalid/bad load file",
    (char*)"bad set statement"
};

//错误信息
static char *SM_ErrorMsg[] = {
    (char*)"Invalid database",
    (char*)"SM Error"
};


//向cerr发送与RM返回码相对应的消息，输入为所需消息的返回码
void SM_PrintError(RC rc) {
    //检查返回码是否在适当的范围内
    if (rc >= START_SM_WARN && rc <= SM_LASTWARN)
        cerr << "SM warning: " << SM_WarnMsg[rc - START_SM_WARN] << "\n";
    //错误代码为负，因此请反转所有内容
    else if (-rc >= -START_SM_ERR && -rc < -SM_LASTERROR)
        cerr << "SM error: " << SM_ErrorMsg[-rc + START_SM_ERR] << "\n";
    else if (rc == PF_UNIX)

#ifdef PC
      cerr << "OS error\n";
#else
      cerr << strerror(errno) << "\n";
#endif
  
    else if (rc == 0)
        cerr << "SM_PrintError called with return code of 0\n";
    else
        cerr << "SM error: " << rc << " is out of bounds\n";
}
