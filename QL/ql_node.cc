
#include <cstdio>
#include <iostream>
#include <unistd.h>
#include "../redbase.h"
#include "../SM/sm.h"
#include "ql.h"
#include "ql_node.h"
#include "node_comps.h"
#include "../IX/comparators.h"

using namespace std;

/**
 * 构造函数
 */
QL_Node::QL_Node(QL_Manager &qlm) : qlm(qlm) {

}

/**
 * 析构函数
 */
QL_Node::~QL_Node(){

}

/**
 * Given a condition, it prints it int he format:
 * relName.attrName <OP> <value or attribute>
 * 格式化输出条件
 */
RC QL_Node::PrintCondition(const Condition condition){
    RC rc = 0;

    // 打印左侧属性
    if(condition.lhsAttr.relName == NULL){
        cout << "NULL";
    }
    else
        cout << condition.lhsAttr.relName;
    cout << "." << condition.lhsAttr.attrName;

    // 打印操作符
    switch(condition.op){
        case EQ_OP : cout << "="; break;
        case LT_OP : cout << "<"; break;
        case GT_OP : cout << ">"; break;
        case LE_OP : cout << "<="; break;
        case GE_OP : cout << ">="; break;
        case NE_OP : cout << "!="; break;
        default: return (QL_BADCOND);
    }
    // 如果右侧是属性
    if(condition.bRhsIsAttr){
        if(condition.rhsAttr.relName == NULL){
            cout << "NULL";
        }
        else
            cout << condition.rhsAttr.relName;
        cout << "." << condition.rhsAttr.attrName;
    }
        // 如果右侧是常量
    else{
        // 根据不同数据类型进行打印·
        if(condition.rhsValue.type == INT){
            print_int(condition.rhsValue.data, 4);
        }
        else if(condition.rhsValue.type == FLOAT){
            print_float(condition.rhsValue.data, 4);
        }
        else{
            print_string(condition.rhsValue.data, strlen((const char *)condition.rhsValue.data));
        }
    }

    return (0);
}


/**
 * 获取给定属性的偏移量和长度，属性由属性列表下标index给出
 */
RC QL_Node::IndexToOffset(int index, int &offset, int &length){
    offset = 0;
    for(int i=0; i < attrsInRecSize; i++){
        if(attrsInRec[i] == index){
            length = qlm.attrEntries[attrsInRec[i]].attrLength;
            return (0);
        }
        offset += qlm.attrEntries[attrsInRec[i]].attrLength;
    }
    return (QL_ATTRNOTFOUND);
}

/**
 * 为节点的条件列表添加条件，其中参数condNum表示条件condition在QL_Manager对象的条件列表下标
 */
RC QL_Node::AddCondition(const Condition condition, int condNum){
    RC rc = 0;
    int index1, index2;
    int offset1, offset2;
    int length1, length2;

    if((rc = qlm.GetAttrCatEntryPos(condition.lhsAttr, index1) ) || (rc = QL_Node::IndexToOffset(index1, offset1, length1)))
        return (rc);

    condList[condIndex].offset1 = offset1;
    condList[condIndex].length = length1;
    condList[condIndex].type = qlm.attrEntries[index1].attrType;

    if(condition.bRhsIsAttr){
        //如果右侧是属性
        if((rc = qlm.GetAttrCatEntryPos(condition.rhsAttr, index2)) || (rc = QL_Node::IndexToOffset(index2, offset2, length2)))
            return (rc);
        condList[condIndex].offset2 = offset2;
        condList[condIndex].isValue = false;
        condList[condIndex].length2 = length2;
    }
    else{
        // 如果右侧是常量
        condList[condIndex].isValue = true;
        condList[condIndex].data = condition.rhsValue.data;
        condList[condIndex].length2 = strlen((char *)condition.rhsValue.data);
    }

    switch(condition.op){
        // 设置操作符
        case EQ_OP : condList[condIndex].comparator = &nequal; break;
        case LT_OP : condList[condIndex].comparator = &nless_than; break;
        case GT_OP : condList[condIndex].comparator = &ngreater_than; break;
        case LE_OP : condList[condIndex].comparator = &nless_than_or_eq_to; break;
        case GE_OP : condList[condIndex].comparator = &ngreater_than_or_eq_to; break;
        case NE_OP : condList[condIndex].comparator = &nnot_equal; break;
        default: return (QL_BADCOND);
    }
    // 设置映射关系
    condsInNode[condIndex] = condNum;
    condIndex++;

    return (0);
}


/**
 * 给定元组数据，检查是否满足该节点中条件列表的条件
 */
RC QL_Node::CheckConditions(char *recData){
    RC rc = 0;
    for(int i = 0; i < condIndex; i++){
        int offset1 = condList[i].offset1;
        // 该条件与常量比较
        if(!condList[i].isValue){
            // 如果不是字符串 或是同等长度的字符串可直接比较
            if(condList[i].type != STRING || condList[i].length == condList[i].length2){
                int offset2 = condList[i].offset2;
                bool comp = condList[i].comparator((void *)(recData + offset1), (void *)(recData + offset2), condList[i].type, condList[i].length);
                if(comp == false){
                    return (QL_CONDNOTMET);
                }
            }
                // 对左右参数长度不同的情况进行进一步比较
            else if(condList[i].length < condList[i].length2){
                int offset2 = condList[i].offset2;
                char *shorter = (char*)malloc(condList[i].length + 1);
                memset((void *)shorter, 0, condList[i].length + 1);
                memcpy(shorter, recData + offset1, condList[i].length);
                shorter[condList[i].length] = '\0';
                bool comp = condList[i].comparator(shorter, (void*)(recData + offset2), condList[i].type, condList[i].length + 1);
                free(shorter);
                if(comp == false)
                    return (QL_CONDNOTMET);
            }
            else{
                int offset2 = condList[i].offset2;
                char *shorter = (char*)malloc(condList[i].length2 + 1);
                memset((void*)shorter, 0, condList[i].length2 + 1);
                memcpy(shorter, recData + offset2, condList[i].length2);
                shorter[condList[i].length2] = '\0';
                bool comp = condList[i].comparator((void*)(recData + offset1), shorter, condList[i].type, condList[i].length2 +1);
                free(shorter);
                if(comp == false)
                    return (QL_CONDNOTMET);
            }
        }
            // 该条件与另一个属性进行比较
        else{
            // 如果不是字符串 或是同等长度的字符串可直接比较
            if(condList[i].type != STRING || condList[i].length == condList[i].length2){
                bool comp = condList[i].comparator((void *)(recData + offset1), condList[i].data,
                                                   condList[i].type, condList[i].length);
                if(comp == false)
                    return (QL_CONDNOTMET);
            }
                // 对左右参数长度不同的情况进行进一步比较
            else if(condList[i].length < condList[i].length2){
                char *shorter = (char*)malloc(condList[i].length + 1);
                memset((void *)shorter, 0, condList[i].length + 1);
                memcpy(shorter, recData + offset1, condList[i].length);
                shorter[condList[i].length] = '\0';
                bool comp = condList[i].comparator(shorter, condList[i].data, condList[i].type, condList[i].length + 1);
                free(shorter);
                if(comp == false)
                    return (QL_CONDNOTMET);
            }
            else{
                char *shorter = (char*)malloc(condList[i].length2 + 1);
                memset((void*)shorter, 0, condList[i].length2 + 1);
                memcpy(shorter, condList[i].data, condList[i].length2);
                shorter[condList[i].length2] = '\0';
                bool comp = condList[i].comparator((void*)(recData + offset1), shorter, condList[i].type, condList[i].length2 +1);
                free(shorter);
                if(comp == false)
                    return (QL_CONDNOTMET);
            }
        }
    }

    return (0);
}

/**
 * 获取指向属性列表得指针以及属性列表长度
 */
RC QL_Node::GetAttrList(int *&attrList, int &attrListSize){
    attrList = attrsInRec;
    attrListSize = attrsInRecSize;
    return (0);
}

/**
 * 获取元组长度
 */
RC QL_Node::GetTupleLength(int &tupleLength){
    tupleLength = this->tupleLength;
    return (0);
}