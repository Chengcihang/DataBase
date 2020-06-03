#include <iostream>
#include "../redbase.h"
#include "../SM/sm.h"
#include "ql.h"
#include "../IX/ix.h"
#include <string>
#include "ql_node.h"


using namespace std;

/**
 * 构造函数
 */
QL_NodeProj::QL_NodeProj(QL_Manager &qlm, QL_Node &prevNode) : QL_Node(qlm), prevNode(prevNode) {
    isOpen = false;
    listsInitialized = false;
    attrsInRecSize = 0;
    tupleLength = 0;
}

/**
 * 析构函数
 */
QL_NodeProj::~QL_NodeProj(){
    if(listsInitialized == true){
        free(attrsInRec);
        free(buffer);
    }
    listsInitialized = false;
}

/**
 * 初始化节点 分配空间
 */
RC QL_NodeProj::SetUpNode(int numAttrToKeep){
    RC rc = 0;
    // 分配属性列表空间
    attrsInRec = (int *)malloc(numAttrToKeep * sizeof(int));
    memset((void*)attrsInRec, 0, sizeof(attrsInRec));
    int attrsInRecSize = 0;

    // 为缓存分配空间
    int bufLength;
    prevNode.GetTupleLength(bufLength);
    buffer = (char *)malloc(bufLength);
    memset((void*) buffer, 0, sizeof(buffer));
    listsInitialized = true;

    return (0);
}

/**
 * 打开迭代器
 */
RC QL_NodeProj::OpenIt(){
    RC rc = 0;
    if((rc = prevNode.OpenIt()))
        return (rc);
    return (0);
}

/**
 * 从上一个节点获取数据，并重建（递归调用后所有节点的数据都存在data所指向的空间中）
 */
RC QL_NodeProj::GetNext(char *data){
    RC rc = 0;
    if((rc = prevNode.GetNext(buffer)))
        return (rc);

    ReconstructRec(data);
    return (0);
}

/**
 * 关闭迭代器
 */
RC QL_NodeProj::CloseIt(){
    RC rc = 0;
    if((rc = prevNode.CloseIt()))
        return (rc);
    return (0);
}

/**
 * 向该节点的属性列表添加一个待投影属性，参数是属性下标
 */
RC QL_NodeProj::AddProj(int attrIndex){
    RC rc = 0;
    tupleLength += qlm.attrEntries[attrIndex].attrLength;

    attrsInRec[attrsInRecSize] = attrIndex;
    attrsInRecSize++;
    return (0);
}

/**
 * 重建节点，只保留上一个节点的属性列表，将属性列表数据存储在参数data所指的内存空间中
 */
RC QL_NodeProj::ReconstructRec(char *data){
    RC rc = 0;
    int currIdx = 0;
    int *attrsInPrevNode;   // 上一个节点的属性列表
    int numAttrsInPrevNode; // 上一个节点属性个数
    // 获取上一个节点的属性列表和属性个数
    if((rc = prevNode.GetAttrList(attrsInPrevNode, numAttrsInPrevNode)))
        return (rc);

    for(int i = 0; i < attrsInRecSize; i++){
        int bufIdx = 0;
        for(int j = 0; j < numAttrsInPrevNode; j++){
            if(attrsInRec[i] == attrsInPrevNode[j]){
                break;
            }
            int prevNodeIdx = attrsInPrevNode[j];
            bufIdx += qlm.attrEntries[prevNodeIdx].attrLength;
        }
        int attrIdx = attrsInRec[i];
        memcpy(data + currIdx, buffer + bufIdx, qlm.attrEntries[attrIdx].attrLength);
        currIdx += qlm.attrEntries[attrIdx].attrLength;
    }

    return (0);
}

/**
 * 打印节点信息（递归调用）打印查询树时调用
 */
RC QL_NodeProj::PrintNode(int numTabs){
    for(int i=0; i < numTabs; i++){
        cout << "\t";
    }
    cout << "--PROJ: \n";
    for(int i = 0; i < attrsInRecSize; i++){
        int index = attrsInRec[i];
        cout << " " << qlm.attrEntries[index].relName << "." << qlm.attrEntries[index].attrName;
    }
    cout << "\n";
    prevNode.PrintNode(numTabs + 1);

    return (0);
}



/**
 * 释放节点内存空间（递归调用）删除查询树时调用
 */
RC QL_NodeProj::DeleteNodes(){
    prevNode.DeleteNodes();
    delete &prevNode;
    if(listsInitialized == true){
        free(attrsInRec);
        free(buffer);
    }
    listsInitialized = false;
    return (0);
}

/**
 * 标志位方法
 */
bool QL_NodeProj::IsRelNode(){
    return false;
}

/**
 * 投影节点不能调用以下函数
 */
RC QL_NodeProj::GetNextRec(RM_Record &rec){
    return (QL_BADCALL);
}

RC QL_NodeProj::OpenIt(void *data){
    return (QL_BADCALL);
}

RC QL_NodeProj::UseIndex(int attrNum, int indexNumber, void *data){
    return (QL_BADCALL);
}
