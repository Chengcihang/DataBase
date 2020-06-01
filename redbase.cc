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
    char *dbname;
    RC rc;

    // Look for 2 arguments.  The first is always the name of the program
    // that was executed, and the second should be the name of the
    // database.
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }

    // 打开数据库
    dbname = argv[1];
    if ((rc = smm.OpenDb(dbname))) {
        PrintError(rc);
        return (1);
    }
    

    RBparse(pfm, smm, qlm);

    // 关闭数据库
    if ((rc = smm.CloseDb())) {
        PrintError(rc);
        return (1);
    }
    

    cout << "Bye.\n";
}
