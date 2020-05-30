//
//  IX_Error.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/14.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef IX_Error_h
#define IX_Error_h

#include <cerrno>
#include <cstdio>
#include <iostream>
#include "ix.h"

using namespace std;

//错误表
static char *IX_WarnMsg[] = {
    (char*)"bad index specification",
    (char*)"bad index name",
    (char*)"invalid index handle",
    (char*)"invalid index file",
    (char*)"page is full",
    (char*)"invalid file",
    (char*)"invalid bucket number",
    (char*)"cannot insert duplicate entry",
    (char*)"invalid scan instance",
    (char*)"invalid record entry",
    (char*)"end of file",
    (char*)"IX warning"
};

static char *IX_ErrorMsg[] = {
    (char*)"IX error"
};

/**
 向cerr发送与RM返回码相对应的消息
 设定PF_UNIX是最后一个有效的RM返回代码
 输入参数：rc为所需消息的返回码
 */
void IX_PrintError(RC rc) {
    //检查返回码是否在合适的范围内
    if (rc >= START_IX_WARN && rc <= IX_LASTWARN)
        cerr << "IX warning: " << IX_WarnMsg[rc - START_IX_WARN] << "\n";
    //错误代码是负数
    else if (-rc >= -START_IX_ERR && -rc < -IX_LASTERROR)
        cerr << "IX error: " << IX_ErrorMsg[-rc + START_IX_ERR] << "\n";
    else if (rc == PF_UNIX)
        
#ifdef PC
      cerr << "OS error\n";
#else
      cerr << strerror(errno) << "\n";
#endif
    
    else if (rc == 0)
        cerr << "IX_PrintError called with return code of 0\n";
    else
        cerr << "IX error: " << rc << " is out of bounds\n";
}



#endif /* IX_Error_h */
