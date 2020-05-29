#include <iostream>
#include "Header/rm_manager.h"

// 测试记录管理模块
int main() {
    // 创建一个底层文件系统
    PF_Manager  *pfManager = new PF_Manager();

    // 创建一个记录管理模块
    RM_Manager rmManager = RM_Manager(*pfManager);

    // 创建一张数据表
    RC rc = OK_RC;
    RM_FileHandle table;
    int recordSize = 10;  // 记录十个字节

    if((rc = rmManager.CreateFile("FirstTable",recordSize))){
        printf("\n文件创建失败...\n");
        RM_PrintError(rc);
    }
    else{
        printf("\n文件FirstTable创建成功...\n");
    }

    // 打开数据表
    if((rc = rmManager.OpenFile("FirstTable",table))){
        printf("\n打开数据表失败...\n");
        RM_PrintError(rc);
    }
    else{
        printf("\n打开数据表成功...\n");
    }

    // 输出缓冲区
    pfManager->PrintBuffer();

    // 关闭数据表
    if((rc = rmManager.CloseFile(table))){
        printf("\n关闭数据表失败...\n");
        RM_PrintError(rc);
    }
    else{
        printf("\n关闭数据表成功...\n");
    }

    // 输出缓冲区
    pfManager->PrintBuffer();

    // 删除数据表
    if((rc = rmManager.DestroyFile("FirstTable"))){
        printf("删除数据表失败...\n");
        RM_PrintError(rc);
    }
    else{
        printf("\n删除数据表成功...\n");
    }


    return 0;
}
