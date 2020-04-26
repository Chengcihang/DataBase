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
    rmManager.CreateFile("FirstTable",recordSize);

    // 打开数据表
    if((rc = rmManager.OpenFile("FirtTable",table)))
        printf("%d",rc);

    // 输出u缓冲区
    pfManager->PrintBuffer();
    return 0;
}
