//
// Created by overload on 5/30/20.
//
#include <iostream>
#include "RM/rm_manager.h"
#include "IX/ix.h"
#include "SM/sm.h"
#include "SM/sm_manager.h"

using namespace std;

PF_Manager pfm;
RM_Manager rmm(pfm);
IX_Manager ixm(pfm);
SM_Manager smm(ixm, rmm);

// 测试记录管理模块
int main() {

    char *dbname;
    RC rc;

    dbname = "testDB";

    smm.DropDb(dbname);
    smm.CreateDb(dbname);

    if ((rc = smm.OpenDb(dbname))) {
        //PrintError(rc);
        printf("打开数据库失败\n");
        //return (1);
    }
    else {
        printf("打开数据库成功\n");
        struct AttrInfo attr = {INT, 4, "num"};
        if (rc = smm.CreateTable("myTestTable", 1, &attr)) {
            printf("建表失败\n");
        }

    }


    // Closes the database folder
    if ((rc = smm.CloseDb())) {
        //PrintError(rc);
        printf("关闭数据库失败\n");
        return (1);
    }

    cout << "Bye.\n";


    return 0;
}