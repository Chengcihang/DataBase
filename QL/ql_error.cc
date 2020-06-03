
#include <cerrno>
#include <cstdio>
#include <iostream>
#include "ql.h"

using namespace std;

// 错误列表
static char *QL_WarnMsg[] = {
        (char*)"bad insert",
        (char*)"duplicate relation",
        (char*)"bad attribute",
        (char*)"attribute not found",
        (char*)"bad select condition",
        (char*)"bad call",
        (char*)"condition not met",
        (char*)"bad update value",
        (char*)"end of iterator"
};

static char *QL_ErrorMsg[] = {
        (char*)"Invalid database",
        (char*)"QL Error"
};


// 打印错误信息
void QL_PrintError(RC rc)
{
    if (rc >= START_QL_WARN && rc <= QL_LASTWARN)
        cerr << "QL warning: " << QL_WarnMsg[rc - START_QL_WARN] << "\n";
    else if (-rc >= -START_QL_ERR && -rc < -QL_LASTERROR)
        cerr << "QL error: " << QL_ErrorMsg[-rc + START_QL_ERR] << "\n";
    else if (rc == PF_UNIX)
#ifdef PC
        cerr << "OS error\n";
#else
        cerr << strerror(errno) << "\n";
#endif
    else if (rc == 0)
        cerr << "QL_PrintError called with return code of 0\n";
    else
        cerr << "QL error: " << rc << " is out of bounds\n";
}
