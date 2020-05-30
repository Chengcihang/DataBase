//
// redbase.h
//   global declarations
//
#ifndef REDBASE_H
#define REDBASE_H

// Please DO NOT include any other files in this file.

//
// Globally-useful defines
//
#define MAXNAME       24                // 属性名的最大长度

//#define MAXCHARLEN  255                 // char类型的最大长度
#define MAXSTRINGLEN 255

#define MAXATTRS      40                // 属性个数最多40个，创建数据表的时候记录信息


#define YY_SKIP_YYWRAP 1
#define yywrap() 1
void yyerror(const char *);

//
// Return codes
//
typedef int RC;

#define OK_RC         0                 // OK_RC return code is guaranteed to always be 0

#define START_PF_ERR  (-1)
#define END_PF_ERR    (-100)
#define START_RM_ERR  (-101)
#define END_RM_ERR    (-200)
#define START_IX_ERR  (-201)
#define END_IX_ERR    (-300)
#define START_SM_ERR  (-301)
#define END_SM_ERR    (-400)
#define START_QL_ERR  (-401)
#define END_QL_ERR    (-500)

#define START_PF_WARN  1
#define END_PF_WARN    100
#define START_RM_WARN  101
#define END_RM_WARN    200
#define START_IX_WARN  201
#define END_IX_WARN    300
#define START_SM_WARN  301
#define END_SM_WARN    400
#define START_QL_WARN  401
#define END_QL_WARN    500


// 当需要将缓冲区的页回写磁盘时需要的参数，若不指定具体页号，则返回所有不被使用的页
const int ALL_PAGES = -2;

//
// 常用属性类型
// Attribute types
//
enum AttrType {
    STRING,
    INT,            // 整型   4字节
    FLOAT,          // 浮点型  4字节
 //   CHAR,           // 字符串，最长MAXCHARLEN个字符
    DATE            // 日期型  8字节1997：04：24 // 年份乘以10000,加月份乘以100,加天
                    // long类型储存
};

//
// Comparison operators
//
enum CompOp {
    NO_OP,                                      // no comparison
    EQ_OP, NE_OP, LT_OP, GT_OP, LE_OP, GE_OP    // binary atomic operators
};

//
// Pin Strategy Hint
//
enum ClientHint {
    NO_HINT                                     // default value
};

//
// TRUE, FALSE and BOOLEAN
//
#ifndef BOOLEAN
typedef char Boolean;
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef NULL
#define NULL 0
#endif

#endif
