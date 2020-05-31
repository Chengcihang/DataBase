#include <iostream>
#include "RM/rm_manager.h"
#include "IX/ix.h"
#include "IX/ix_indexhandle.h"
#include "SM/sm_manager.h"

// 测试记录管理模块
int main() {
    // 创建一个底层文件系统
    PF_Manager  *pfManager = new PF_Manager();

    // 创建一个记录管理模块
    RM_Manager rmManager = RM_Manager(*pfManager);

    //创建一个索引管理器
    IX_Manager ixManager = IX_Manager(*pfManager);

    //创建一个系统管理器
    SM_Manager smManager = SM_Manager(ixManager, rmManager);

    // 创建一张数据表
    RC rc = OK_RC;
    RM_FileHandle table;
    int recordSize = 10;  // 记录十个字节

    if (rc = ixManager.CreateIndex("indexFile", 1, INT, 4)) {
        printf("\n索引文件创建失败！\n");
    }
    else {
        printf("\n索引文件indexFile创建成功！\n");

        IX_IndexHandle ixHandle = IX_IndexHandle();
        if (rc = ixManager.OpenIndex("indexFile", 1, ixHandle)) {
            printf("\n1索引文件打开失败！\n");
        }
        else {
            printf("\n1索引文件indexFile打开成功！\n");
        }

        ixManager.CloseIndex(ixHandle);

        if (rc = ixManager.DestroyIndex("indexFile", 1)) {
            printf("\n索引文件销毁失败！\n");
        }
        else {
            printf("\n索引文件indexFile销毁成功！\n");

            if (rc = ixManager.OpenIndex("indexFile", 1, ixHandle)) {
                printf("\n2索引文件打开失败！\n");
            }
            else {
                printf("\n2索引文件indexFile打开成功！\n");
            }

        }
        /*
        IX_IndexHandle ixHandle = IX_IndexHandle();
        if (rc = ixManager.OpenIndex("indexFile", 1, ixHandle)) {
            printf("\n索引文件打开失败！\n");
        }
        else {
            printf("\n索引文件indexFile打开成功！\n");
            for (int i = 0; i < 10; i++) {
                printf("\n插入%d\n", i);
                ixHandle.InsertEntry(&i, RID(i, 1));
            }
            printf("\n打印结果\n");
            ixHandle.PrintIndex();
        }
        */
    }

    return 0;
}


