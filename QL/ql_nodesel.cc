#include <iostream>
#include "../redbase.h"
#include "../SM/sm.h"
#include "ql.h"
#include <string>
#include "ql_node.h"

using namespace std;

/**
 * 构造函数
 */
QL_NodeSel::QL_NodeSel(QL_Manager &qlm, QL_Node &prevNode) : QL_Node(qlm), prevNode(prevNode) {
    isOpen = false;
    listsInitialized = false;
    tupleLength = 0;
    attrsInRecSize = 0;
    condIndex = 0;
    attrsInRecSize = 0;
}

/**
 * 析构函数
 */
QL_NodeSel::~QL_NodeSel(){
    if(listsInitialized == true){
        free(condList);
        free(attrsInRec);
        free(buffer);
        free(condsInNode);
    }
    listsInitialized = false;
}

/**
 * 初始化节点
 */
RC QL_NodeSel::SetUpNode(int numConds){
    RC rc = 0;
    int *attrListPtr;
    // 获取前序节点的属性列表
    if((rc = prevNode.GetAttrList(attrListPtr, attrsInRecSize)))
        return (rc);
    // 分配当前节点属性列表内存空间并赋值
    attrsInRec = (int *)malloc(attrsInRecSize*sizeof(int));
    for(int i = 0;  i < attrsInRecSize; i++){
        attrsInRec[i] = attrListPtr[i];
    }

    // 分配条件列表空间并初始化
    condList = (Cond *)malloc(numConds * sizeof(Cond));
    for(int i= 0; i < numConds; i++){
        condList[i] = {0, NULL, true, NULL, 0, 0, INT};
    }
    condsInNode = (int*)malloc(numConds * sizeof(int));
    memset((void*)condsInNode, 0, sizeof(condsInNode));

    // 初始化缓冲区
    prevNode.GetTupleLength(tupleLength);
    buffer = (char *)malloc(tupleLength);
    memset((void*)buffer, 0, sizeof(buffer));
    listsInitialized = true;
    return (0);
}



/**
 * 打开迭代器
 */
RC QL_NodeSel::OpenIt(){
    RC rc = 0;
    if((rc = prevNode.OpenIt()))
        return (rc);
    return (0);
}

/**
 * 获取下一个满足条件的记录
 */
RC QL_NodeSel::GetNext(char *data){
    RC rc = 0;
    while(true){
        if((rc = prevNode.GetNext(buffer))){
            return (rc);
        }
        // 找到满足条件的记录
        RC cond = CheckConditions(buffer);
        if(cond == 0)
            break;
    }
    memcpy(data, buffer, tupleLength);
    return (0);
}

/**
 * 关闭迭代器
 */
RC QL_NodeSel::CloseIt(){
    RC rc = 0;
    if((rc = prevNode.CloseIt()))
        return (rc);

    return (0);
}

/**
 * 获取前序节点的下一条记录
 */
RC QL_NodeSel::GetNextRec(RM_Record &rec){
    RC rc = 0;
    while(true){
        // 获取上个节点的记录
        if((rc = prevNode.GetNextRec(rec)))
            return (rc);
        // 获取记录中的数据
        char *pData;
        if((rc = rec.GetData(pData)))
            return (rc);
        // 判断是否满足节点条件
        RC cond = CheckConditions(pData);
        if(cond == 0)
            break;
    }

    return (0);
}


/**
、 * 打印节点信息，递归调用（打印查询树时使用）
 */
RC QL_NodeSel::PrintNode(int numTabs){
    for(int i=0; i < numTabs; i++){
        cout << "\t";
    }
    cout << "--SEL: " << endl;
    for(int i = 0; i < condIndex; i++){
        for(int j=0; j <numTabs; j++){
            cout << "\t";
        }
        PrintCondition(qlm.condptr[condsInNode[i]]);
        cout << "\n";
    }
    prevNode.PrintNode(numTabs + 1);

    return (0);
}

/**
、 * 释放节点的内存空间，递归调用（删除查询树时使用）
 */
RC QL_NodeSel::DeleteNodes(){
    prevNode.DeleteNodes();
    delete &prevNode;
    if(listsInitialized == true){
        free(condList);
        free(attrsInRec);
        free(buffer);
        free(condsInNode);
    }
    listsInitialized = false;
    return (0);
}

/**
 * 返回标志位
 */
bool QL_NodeSel::IsRelNode(){
    return false;
}

/**
 * 选择节点不能调用以下函数
 */
RC QL_NodeSel::OpenIt(void *data){
    return (QL_BADCALL);
}

RC QL_NodeSel::UseIndex(int attrNum, int indexNumber, void *data){
    return (QL_BADCALL);
}
