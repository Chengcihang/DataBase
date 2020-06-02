//
// Created by overload on 5/30/20.
//
#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "redbase.h"
#include "RM/rm.h"
#include "QL/ql.h"
#include "RM/rm_manager.h"
#include "SM/sm_manager.h"

using namespace std;

PF_Manager pfm;
RM_Manager rmm(pfm);
IX_Manager ixm(pfm);
SM_Manager smm(ixm, rmm);
QL_Manager qlm(smm, ixm, rmm);

// 测试记录管理模块
int main() {
    char *dbname;
    RC rc;

    dbname = "testDB";
    smm.DropDb(dbname);
    smm.CreateDb(dbname);
    smm.OpenDb(dbname);

    struct AttrInfo attr = {INT, 4, "num"};
    rc = smm.CreateTable("table1",1, &attr);

    rc = smm.DropTable("table1");
    PrintError(rc);

//    RBparse(pfm, smm, qlm);

    // Closes the database folder
    if ((rc = smm.CloseDb())) {
        PrintError(rc);
        return (1);
    }


    cout << "Bye.\n";

}