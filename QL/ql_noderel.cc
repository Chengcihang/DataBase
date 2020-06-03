#include <iostream>
#include "../redbase.h"
#include "../SM/sm.h"
#include "ql.h"
#include <string>
#include "ql_node.h"
#include "../IX/comparators.h"


using namespace std;

/**
 * 构造函数
 */
QL_NodeRel::QL_NodeRel(QL_Manager &qlm, RelCatEntry *rEntry) : QL_Node(qlm){
    relName = (char *)malloc(MAXNAME+1);
    memset((void *)relName, 0, sizeof(relName));
    memcpy(this->relName, rEntry->relName, strlen(rEntry->relName) + 1);
    relNameInitialized = true;
    listsInitialized = false;

    isOpen = false;
    tupleLength = rEntry->tupleLength;

    useIndex = false;
    indexNo = 0;
    indexAttr = 0;
    void *value = NULL;
    useIndexJoin = false;
}

/**
 * Delete the memory allocated at this node
 * 析构函数
 */
QL_NodeRel::~QL_NodeRel(){
    if(relNameInitialized == true){
        free(relName);
    }
    relNameInitialized = false;
    if(listsInitialized == true){
        free(attrsInRec);
    }
    listsInitialized = false;
}

/**
 * 通过打开文件扫描器或索引扫描器获取迭代器
 */
RC QL_NodeRel::OpenIt(){
    RC rc = 0;
    isOpen = true;
    // 如果使用索引
    if(useIndex){
        if((rc = qlm.ixm.OpenIndex(relName, indexNo, ih)))
            return (rc);
        if((rc = is.OpenScan(ih, EQ_OP, value)))
            return (rc);
        if((rc = qlm.rmm.OpenFile(relName, fh)))
            return (rc);
    }
    else{
        if((rc = qlm.rmm.OpenFile(relName, fh)))
            return (rc);
        if((rc = fs.OpenScan(fh, INT, 4, 0, NO_OP, NULL)))
            return (rc);
    }
    return (0);
}

/**
 * 打开关系表文件、索引，打开扫描以获取迭代器
 */
RC QL_NodeRel::OpenIt(void *data){
    RC rc = 0;
    isOpen = true;
    value = data;
    if((rc = qlm.ixm.OpenIndex(relName, indexNo, ih)))
        return (rc);
    if((rc = is.OpenScan(ih, EQ_OP, value)))
        return (rc);
    if((rc = qlm.rmm.OpenFile(relName, fh)))
        return (rc);
    return (0);
}

/**
 * 令指针指向将下一条记录的数据
 */
RC QL_NodeRel::GetNext(char *data){
    RC rc = 0;
    char *recData;
    RM_Record rec;
    // 获取下一条记录的数据
    if((rc = RetrieveNextRec(rec, recData))){
        if(rc == RM_EOF || rc == IX_EOF){
            return (QL_EOI);
        }
        return (rc);
    }
    memcpy(data, recData, tupleLength);
    return (0);
}

/**
 * 通过关闭文件扫描器或索引扫描器关闭迭代器
 */
RC QL_NodeRel::CloseIt(){
    RC rc = 0;
    if(useIndex){
        if((rc = qlm.rmm.CloseFile(fh)))
            return (rc);
        if((rc = is.CloseScan()))
            return (rc);
        if((rc = qlm.ixm.CloseIndex(ih )))
            return (rc);
    }
    else{
        if((rc = fs.CloseScan()) || (rc = qlm.rmm.CloseFile(fh)))
            return (rc);
    }
    isOpen = false;
    return (rc);
}

/**
 * 获取下一条记录和数据（有索引使用索引，无索引则直接扫描）
 */
RC QL_NodeRel::RetrieveNextRec(RM_Record &rec, char *&recData){
    RC rc = 0;
    // 有索引的情况
    if(useIndex){
        RID rid;
        if((rc = is.GetNextEntry(rid) ))
            return (rc);
        if((rc = fh.GetRec(rid, rec) ))
            return (rc);
    }
        // 无索引的情况
    else{
        if((rc = fs.GetNextRec(rec)))
            return (rc);
    }
    // 获取记录的数据
    if((rc = rec.GetData(recData)))
        return (rc);
    return (0);
}

/**
 * 从关系表中获取下一条记录
 */
RC QL_NodeRel::GetNextRec(RM_Record &rec){
    RC rc = 0;
    char *recData;
    if((rc = RetrieveNextRec(rec, recData))){
        if(rc == RM_EOF || rc == IX_EOF)
            return (QL_EOI);
        return (rc);
    }
    return (0);
}


/**
 * 令该节点使用索引遍历关系表
 */
RC QL_NodeRel::UseIndex(int attrNum, int indexNumber, void *data){
    indexNo = indexNumber;
    value = data;
    useIndex = true;
    indexAttr = attrNum;
    return (0);
}

/**
 * 初始化节点的属性列表和属性个数变量
 */
RC QL_NodeRel::SetUpNode(int *attrs, int attrlistSize){
    RC rc = 0;
    attrsInRecSize = attrlistSize;
    attrsInRec = (int*)malloc(attrlistSize*sizeof(int));
    memset((void *)attrsInRec, 0, sizeof(attrsInRec));
    for(int i = 0; i < attrlistSize; i++){
        attrsInRec[i] = attrs[i];
    }
    listsInitialized = true;

    return (rc);
}

/**
 * 输出节点信息
 */
RC QL_NodeRel::PrintNode(int numTabs){
    for(int i=0; i < numTabs; i++){
        cout << "\t";
    }
    cout << "--REL: " << relName;
    if(useIndex && !useIndexJoin){
        cout << " using index on attribute " << qlm.attrEntries[indexAttr].attrName;
        if(value == NULL){
            cout << endl;
        }
        else{
            cout << " = ";

            if(qlm.attrEntries[indexAttr].attrType == INT){
                print_int(value, 4);
            }
            else if(qlm.attrEntries[indexAttr].attrType == FLOAT){
                print_float(value, 4);
            }
            else{
                print_string(value, strlen((char *)value));
            }
            cout << "\n";
        }
    }
    else if(useIndexJoin && useIndex){
        cout << " using index join on attribute " <<qlm.attrEntries[indexAttr].attrName << endl;
    }
    else{
        cout << " using filescan." << endl;
    }
    return (0);
}

/**
 * 释放该节点有关的所有内存空间
 */
RC QL_NodeRel::DeleteNodes(){
    if(relNameInitialized == true){
        free(relName);
    }
    relNameInitialized = false;
    if(listsInitialized == true){
        free(attrsInRec);
    }
    listsInitialized = false;
    return (0);
}

/**
 * 返回是否是关系节点的标志位
 */
bool QL_NodeRel::IsRelNode(){
    return true;
}

