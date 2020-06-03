//
// redbase.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.

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


//
// main
//
int main(int argc, char *argv[])
{
    char const *dbname = "testDB";
    RC rc;

    // 创建数据库
    dbname = "testDB";
    smm.DropDb(dbname);
    smm.CreateDb(dbname);
    smm.OpenDb(dbname);

    struct AttrInfo attr = {INT, 4, "num"};
    rc = smm.CreateTable("table1",1, &attr);

    rc = smm.DropTable("table1");

    // Closes the database folder
    if ((rc = smm.CloseDb())) {
        PrintError(rc);
        return (1);
    }

    // 打开数据库
    if ((rc = smm.OpenDb(dbname))) {
        PrintError(rc);
        return (1);
    } else{
        printf("打开数据库：%s\n",dbname);
    }


    RBparse(pfm, smm, qlm);

    // 关闭数据库
    if ((rc = smm.CloseDb())) {
        PrintError(rc);
        return (1);
    }else{
        printf("关闭数据库：%s\n",dbname);
    }


    cout << "Bye.\n";
}
