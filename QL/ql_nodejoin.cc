
#include <iostream>
#include <unistd.h>
#include "../redbase.h"
#include "../SM/sm.h"
#include "ql.h"
#include <string>
#include "ql_node.h"


using namespace std;

/**
 * 构造函数
 */
QL_NodeJoin::QL_NodeJoin(QL_Manager &qlm, QL_Node &node1, QL_Node &node2) :
        QL_Node(qlm), node1(node1), node2(node2){
    isOpen = false;
    listsInitialized = false;
    attrsInRecSize = 0;
    tupleLength = 0;
    condIndex = 0;
    firstNodeSize = 0;
    gotFirstTuple = false;
    useIndexJoin = false;
}

/**
 * 析构函数
 */
QL_NodeJoin::~QL_NodeJoin(){
    if(listsInitialized = true){
        free(attrsInRec);
        free(condList);
        free(buffer);
        free(condsInNode);
    }
}

/**
 * 初始化节点
 */
RC QL_NodeJoin::SetUpNode(int numConds){
    RC rc = 0;
    // 取出前两个节点的属性列表放入当前连接节点的属性列表中
    int *attrList1;
    int *attrList2;
    int attrListSize1;
    int attrListSize2;
    // 获取前两个节点的属性列表与长度
    if((rc = node1.GetAttrList(attrList1, attrListSize1)) || (rc = node2.GetAttrList(attrList2, attrListSize2)))
        return (rc);
    attrsInRecSize = attrListSize1 + attrListSize2;
    // 为当前节点属性列表分配空间并赋值
    attrsInRec = (int*)malloc(attrsInRecSize*sizeof(int));
    memset((void *)attrsInRec, 0, sizeof(attrsInRec));
    for(int i = 0; i < attrListSize1; i++){
        attrsInRec[i] = attrList1[i];
    }
    for(int i=0; i < attrListSize2; i++){
        attrsInRec[attrListSize1+i] = attrList2[i];
    }

    // 分配条件列表的内存空间
    condList = (Cond *)malloc(numConds * sizeof(Cond));
    for(int i= 0; i < numConds; i++){
        condList[i] = {0, NULL, true, NULL, 0, 0, INT};
    }
    condsInNode = (int*)malloc(numConds * sizeof(int));

    // 获取前序节点的元组长度
    int tupleLength1, tupleLength2;
    node1.GetTupleLength(tupleLength1);
    node2.GetTupleLength(tupleLength2);
    tupleLength = tupleLength1 + tupleLength2;
    firstNodeSize = tupleLength1;

    // 为缓存分配空间，用于存储前序节点的元组
    buffer = (char *)malloc(tupleLength);
    memset((void*)buffer, 0, sizeof(buffer));
    listsInitialized = true;
    return (0);
}

/**
 * 返回下一个满足条件的元组数据
 */
RC QL_NodeJoin::GetNext(char *data){
    RC rc = 0;
    // 获取第一个元组作为迭代器的开头
    if(gotFirstTuple == false && ! useIndexJoin){
        if((rc = node1.GetNext(buffer)))
            return (rc);
    }
    else if(gotFirstTuple == false && useIndexJoin){
        if((rc = node1.GetNext(buffer)))
            return (rc);
        int offset, length;
        IndexToOffset(indexAttr, offset, length);
        if((rc = node2.OpenIt(buffer + offset)))
            return (rc);
    }
    gotFirstTuple = true;
    while(true && !useIndexJoin){
        if((rc = node2.GetNext(buffer + firstNodeSize)) && rc == QL_EOI){
            if((rc = node1.GetNext(buffer)))
                return (rc);
            if((rc = node2.CloseIt()) || (rc = node2.OpenIt()))
                return (rc);
            if((rc = node2.GetNext(buffer + firstNodeSize)))
                return (rc);
        }
        // 找到满足条件的元组
        RC comp = CheckConditions(buffer);
        if(comp == 0)
            break;
    }
    while(true && useIndexJoin){
        if((rc = node2.GetNext(buffer + firstNodeSize)) ){
            int found = false;
            while(found == false){
                if((rc = node1.GetNext(buffer)))
                    return (rc);
                int offset, length;
                IndexToOffset(indexAttr, offset, length);
                if((rc = node2.CloseIt()) || (rc = node2.OpenIt(buffer + offset)))
                    return (rc);
                if((rc = node2.GetNext(buffer + firstNodeSize)) && rc == QL_EOI){
                    found = false;
                }
                else
                    found = true;
            }
        }
        RC comp = CheckConditions(buffer);
        if (comp == 0)
            break;
    }
    // 将满足条件的元组存入data中
    memcpy(data, buffer, tupleLength);
    return (0);
}

/**
 * 打开迭代器
 */
RC QL_NodeJoin::OpenIt(){
    RC rc = 0;
    if(!useIndexJoin){
        if((rc = node1.OpenIt()) || (rc = node2.OpenIt()))
            return (rc);
    }
    else{
        if((rc = node1.OpenIt() ))
            return (rc);
    }
    gotFirstTuple = false;

    return (0);
}

/**
 * 关闭迭代器
 */
RC QL_NodeJoin::CloseIt(){
    RC rc = 0;
    if((rc = node1.CloseIt()) || (rc = node2.CloseIt()))
        return (rc);
    gotFirstTuple = false;
    return (0);
}

/**
 * 打印节点信息（递归调用）（打印查询计划时使用）
 */
RC QL_NodeJoin::PrintNode(int numTabs){
    for(int i=0; i < numTabs; i++){
        cout << "\t";
    }
    cout << "--JOIN: \n";
    for(int i = 0; i < condIndex; i++){
        for(int j=0; j <numTabs; j++){
            cout << "\t";
        }
        PrintCondition(qlm.condptr[condsInNode[i]]);
        cout << "\n";
    }
    node1.PrintNode(numTabs + 1);
    node2.PrintNode(numTabs + 1);
    return (0);
}

/**
 * 释放当前节点和前两个节点内存空间（递归调用）（删除操查询树时调用）
 */
RC QL_NodeJoin::DeleteNodes(){
    node1.DeleteNodes();
    node2.DeleteNodes();
    delete &node1;
    delete &node2;
    if(listsInitialized == true){
        free(attrsInRec);
        free(condList);
        free(condsInNode);
        free(buffer);
    }
    listsInitialized = false;
    return (0);
}

RC QL_NodeJoin::UseIndexJoin(int indexAttr, int subNodeAttr, int indexNumber){
    useIndexJoin = true;
    this->indexAttr = indexAttr;
    node2.UseIndex(subNodeAttr, indexNumber, NULL);
    node2.useIndexJoin= true;
    return (0);
}

/**
 * 返回标志位
 */
bool QL_NodeJoin::IsRelNode(){
    return false;
}

/**
 * 连接节点不能调用以下函数
 */
RC QL_NodeJoin::GetNextRec(RM_Record &rec){
    return (QL_BADCALL);
}

RC QL_NodeJoin::OpenIt(void *data){
    return (QL_BADCALL);
}

RC QL_NodeJoin::UseIndex(int attrNum, int indexNumber, void *data){
    return (QL_BADCALL);
}

