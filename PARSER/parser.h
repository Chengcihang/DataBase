//
//  Parser.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/28.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef Parser_h
#define Parser_h

#include <cstdio>
#include <iostream>
#include "../redbase.h"
#include "../PF/pf.h"

//声明结构

//属性信息
struct AttrInfo{
    //类型
    AttrType attrType;
    //长度
    int attrLength;
    //名称
    char *attrName;
};


//属性信息
struct RelAttr{
    //关系名称
    char *relName;
    //属性名称
    char *attrName;
    //打印函数
    friend std::ostream &operator<<(std::ostream &s, const RelAttr &ra);
};


//值
struct Value {
    //属性类型
    AttrType type;
    //数据值
    void *data;
    //打印函数
    friend std::ostream &operator<<(std::ostream &s, const Value &v);
};


//条件
struct Condition{
    //左侧属性
    RelAttr lhsAttr;
    //比较运算符
    CompOp op;
    //rhs是属性的标志位，如果为true下面的rhsAttr有效，否则，下面的rhsValue有效
    int bRhsIsAttr;
    //右侧属性
    RelAttr rhsAttr;
    //右侧值
    Value rhsValue;
    //打印函数
    friend std::ostream &operator<<(std::ostream &s, const Condition &c);

};


std::ostream &operator<<(std::ostream &s, const CompOp &op);
std::ostream &operator<<(std::ostream &s, const AttrType &at);


//解析函数
class QL_Manager;
class SM_Manager;

void RBparse(PF_Manager &pfm, SM_Manager &smm, QL_Manager &qlm);

//错误打印功能
void PrintError(RC rc);

// bQueryPlans由parse.y分配
//当bQueryPlans为1时，将显示为SFW查询选择的查询计划。 bQueryPlans为0时，则不显示查询计划
extern int bQueryPlans;


#endif /* Parser_h */
