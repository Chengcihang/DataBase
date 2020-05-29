//
//  Index.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/14.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef Index_h
#define Index_h

#include "redbase.h"
#include "../Header/rm_rid.h"
#include "../Header/pf.h"
#include <string>
#include <stdlib.h>
#include <cstring>

#include "indexheader.h"
#include "ix_indexhandle.h"
#include "ix_indexscan.h"
#include "ix_manager.h"


//打印错误功能
void IX_PrintError(RC rc);

//索引模块用到的错误变量的宏定义（这里直接用了斯坦福源码的定义）
//索引文件规范错误
#define IX_BADINDEXSPEC         (START_IX_WARN + 0)
//错误的索引名称
#define IX_BADINDEXNAME         (START_IX_WARN + 1)
//使用的FileHandle无效
#define IX_INVALIDINDEXHANDLE   (START_IX_WARN + 2)
//错误的索引文件
#define IX_INVALIDINDEXFILE     (START_IX_WARN + 3)
//文件中的一个结点已满
#define IX_NODEFULL             (START_IX_WARN + 4)
//错误的文件名
#define IX_BADFILENAME          (START_IX_WARN + 5)
//尝试访问的存储区无效
#define IX_INVALIDBUCKET        (START_IX_WARN + 6)
//尝试输入重复项
#define IX_DUPLICATEENTRY       (START_IX_WARN + 7)
//无效的IX_Indexscsan
#define IX_INVALIDSCAN          (START_IX_WARN + 8)
//项不在索引中
#define IX_INVALIDENTRY         (START_IX_WARN + 9)
//索引文件的结尾
#define IX_EOF                  (START_IX_WARN + 10)
#define IX_LASTWARN             IX_EOF

//错误
#define IX_ERROR                (START_IX_ERR - 0)
#define IX_LASTERROR            IX_ERROR


#endif /* Index_h */
