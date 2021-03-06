
#include <iostream>
#include <cassert>
#include "../redbase.h"
#include "ql.h"
#include "../SM/sm.h"
#include "ql_node.h"
#include <set>
#include <map>
#include <cfloat>
#include "../QO/qo.h"

#undef max
#undef min
#include <algorithm>

using namespace std;

/**
 * QL_Manager的构造函数
 */
QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm) : rmm(rmm), ixm(ixm), smm(smm)
{
    assert (&smm && &ixm && &rmm);
}

/**
 * QL_Manager的析构函数
 */
QL_Manager::~QL_Manager()
{

}

/**
 * 初始化各参数 准备执行下一条命令
 * 在每次执行操作前调用
 */
RC QL_Manager::Reset(){
    relToInt.clear();
    relToAttrIndex.clear();
    attrToRel.clear();
    conditionToRel.clear();
    nAttrs = 0;
    nRels = 0;
    nConds = 0;
    condptr = NULL;
    isUpdate = false;
    return (0);
}

/**
 * 处理Select命令
 */
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[])
{
    int i;
    RC rc = 0;
    if(smm.printPageStats){
        smm.ResetPageStats();
    }
    //格式化输出解析后的命令
    cout << "Select\n";

    cout << "   nSelAttrs = " << nSelAttrs << "\n";
    for (i = 0; i < nSelAttrs; i++)
        cout << "   selAttrs[" << i << "]:" << selAttrs[i] << "\n";

    cout << "   nRelations = " << nRelations << "\n";
    for (i = 0; i < nRelations; i++)
        cout << "   relations[" << i << "] " << relations[i] << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    //检查关系表名是否有重复
    if((rc = ParseRelNoDup(nRelations, relations)))
        return (rc);

    // 重置各参数
    Reset();
    nRels = nRelations;
    nConds = nConditions;
    condptr = conditions;

    //获取各关系表
    relEntries = (RelCatEntry *)malloc(sizeof(RelCatEntry)*nRelations);
    memset((void*)relEntries, 0, sizeof(*relEntries));
    if((rc = smm.GetAllRels(relEntries, nRelations, relations, nAttrs, relToInt))){
        free(relEntries);
        return (rc);
    }

    // 计算各参数
    attrEntries = (AttrCatEntry *)malloc(sizeof(AttrCatEntry)*nAttrs);
    int slot = 0;
    for(int i=0; i < nRelations; i++){
        string relString(relEntries[i].relName);
        relToAttrIndex.insert({relString, slot});
        if((rc = smm.GetAttrForRel(relEntries + i, attrEntries + slot, attrToRel))){
            free(relEntries);
            free(attrEntries);
            return (rc);
        }
        slot += relEntries[i].attrCount;
    }

    // 检查select涉及的属性是否合法 若不合法释放变量空间
    if((rc = ParseSelectAttrs(nSelAttrs, selAttrs))){
        free(relEntries);
        free(attrEntries);
        return (rc);
    }
    // 检查select涉及的条件是否合法 若不合法释放变量空间
    if((rc = ParseConditions(nConditions, conditions))){
        free(relEntries);
        free(attrEntries);
        return (rc);
    }

    QL_Node *topNode;
    float cost, tupleEst;
    // 如果使用查询优化器
    if(smm.useQO){
        QO_Manager *qom = new QO_Manager(*this, nRels, relEntries, nAttrs, attrEntries,
                                         nConds, condptr);
        QO_Rel * qorels = (QO_Rel*)(malloc(sizeof(QO_Rel)*nRels));
        for(int i=0; i < nRels; i++){
            *(qorels + i) = (QO_Rel){ 0, -1, -1};
            *(qorels + i) = (QO_Rel){ 0, -1, -1};
        }
        qom->Compute(qorels, cost, tupleEst);   //计算QO_Rel数组中各节点值 并计算预估cost与元组数
        qom->PrintRels();
        RecalcCondToRel(qorels);
        // 使用查询优化器建立查询树
        if((rc = SetUpNodesWithQO(topNode, qorels, nSelAttrs, selAttrs)))
            return (rc);

        delete qom;
        free(qorels);
    }
    else{
        // 不使用查询优化器建立查询树
        if((rc = SetUpNodes(topNode, nSelAttrs, selAttrs)))
            return (rc);
    }

    // 执行select查询 参数是查询树的根节点
    if((rc = RunSelect(topNode)))
        return (rc);

    // 如果使用查询优化器 输出预测数据
    if(smm.useQO){
        cout << "estimated cost: " << cost << endl;
        cout << "estimated # tuples: " << tupleEst << endl;
    }

    // 根据标志位显示查询计划
    if(bQueryPlans){
        cout << "PRINTING QUERY PLAN" <<endl;
        topNode->PrintNode(0);
    }

    // 递归删除查询树
    if((rc = CleanUpNodes(topNode)))
        return (rc);

    // 根据标志位打印页面统计信息
    if(smm.printPageStats){
        cout << endl;
        smm.PrintPageStats();
    }

    free(relEntries);
    free(attrEntries);

    return (rc);
}


/**
 * 根据参数给定的查询树头节点进行查询 并打印所有符合条件的元组
 */
RC QL_Manager::RunSelect(QL_Node *topNode){
    RC rc = 0;
    int finalTupLength;
    topNode->GetTupleLength(finalTupLength);
    int *attrList;
    int attrListSize;
    if((rc = topNode->GetAttrList(attrList, attrListSize)))
        return (rc);

    // 初始化属性列表
    DataAttrInfo * attributes = (DataAttrInfo *)malloc(attrListSize* sizeof(DataAttrInfo));
    if((rc = SetUpPrinter(topNode, attributes)))
        return (rc);
    Printer printer(attributes, attrListSize);
    printer.PrintHeader(cout);

    // 打开迭代器
    if((rc = topNode->OpenIt()))
        return (rc);
    RC it_rc = 0;
    char *buffer = (char *)malloc(finalTupLength);
    // 遍历所有满足条件的纪录
    it_rc = topNode->GetNext(buffer);
    while(it_rc == 0){
        printer.Print(cout, buffer);
        it_rc = topNode->GetNext(buffer);
    }

    printer.PrintFooter(cout);
    free(buffer);
    free(attributes);
    if((rc = topNode->CloseIt()))
        return (rc);

    return (0);
}


/**
 * 用于检测属性是否合法，属性在参数中给出，这一函数基于初始化时获取查询中关系表和属性所获得的map进行判断
 */
bool QL_Manager::IsValidAttr(const RelAttr attr){
    if(attr.relName != NULL){
        string relString(attr.relName);
        string attrString(attr.attrName);
        map<string, int>::iterator it = relToInt.find(relString);
        map<string, set<string> >::iterator itattr = attrToRel.find(attrString);

        // 成功找到
        if(it != relToInt.end() && itattr != attrToRel.end()){
            set<string> relNames = (*itattr).second;
            set<string>::iterator setit = relNames.find(relString);
            if(setit != relNames.end())
                return true;
            else
                return false;
        }
        else
            //没找到
            return false;
    }
    else{
        string attrString(attr.attrName);
        set<string> relNames = attrToRel[attrString];
        if(relNames.size() == 1){
            return true;
        }
        else{
            return false;
        }
    }
}


/**
 * 建立查询树，返回指向根节点的指针topNpde
 */
RC QL_Manager::SetUpNodes(QL_Node *&topNode, int nSelAttrs, const RelAttr selAttrs[]){
    RC rc = 0;
    // 建立第一个节点
    if((rc = SetUpFirstNode(topNode)))
        return (rc);

    // 连接其他关系表
    QL_Node* currNode;
    currNode = topNode;
    for(int i = 1; i < nRels; i++){
        if((rc = JoinRelation(topNode, currNode, i)))
            return (rc);
        currNode = topNode;
    }

    // 如果是select * 则不添加投影节点
    if((nSelAttrs == 1 && strncmp(selAttrs[0].attrName, "*", strlen(selAttrs[0].attrName)) == 0))
        return (0);
    // 否则建立投影节点
    QL_NodeProj *projNode = new QL_NodeProj(*this, *currNode);
    projNode->SetUpNode(nSelAttrs);
    for(int i= 0 ; i < nSelAttrs; i++){
        int attrIndex = 0;
        if((rc = GetAttrCatEntryPos(selAttrs[i], attrIndex)))
            return (rc);
        if((rc = projNode->AddProj(attrIndex)))
            return (rc);
    }
    topNode = projNode;

    return (rc);
}

/**
 * 使用查询优化器建立节点
 */
RC QL_Manager::SetUpNodesWithQO(QL_Node *&topNode, QO_Rel* qorels, int nSelAttrs, const RelAttr selAttrs[]){
    RC rc = 0;
    if((rc = SetUpFirstNodeWithQO(topNode, qorels)))
        return (rc);

    // For all other relations, join it, left-deep style, with the previously
    // seen relations
    QL_Node* currNode;
    currNode = topNode;
    for(int i = 1; i < nRels; i++){
        if((rc = JoinRelationWithQO(topNode, qorels, currNode, i)))
            return (rc);
        currNode = topNode;
    }

    // If select *, don't add project nodes
    if((nSelAttrs == 1 && strncmp(selAttrs[0].attrName, "*", strlen(selAttrs[0].attrName)) == 0))
        return (0);
    // Otherwise, add a project node, and give it the attributes to project
    QL_NodeProj *projNode = new QL_NodeProj(*this, *currNode);
    projNode->SetUpNode(nSelAttrs);
    for(int i= 0 ; i < nSelAttrs; i++){
        int attrIndex = 0;
        if((rc = GetAttrCatEntryPos(selAttrs[i], attrIndex)))
            return (rc);
        if((rc = projNode->AddProj(attrIndex)))
            return (rc);
    }
    topNode = projNode;
    return (rc);
}

/**
 * 将新的关系表与根节点连接并返回新的跟节点
 */
RC QL_Manager::JoinRelation(QL_Node *&topNode, QL_Node *currNode, int relIndex){
    RC rc = 0;
    bool useIndex = false;
    // 创建新的关系节点
    QL_NodeRel *relNode = new QL_NodeRel(*this, relEntries + relIndex);

    // Set up the list of indices corresponding to attributes in the relation
    int *attrList = (int *)malloc(relEntries[relIndex].attrCount * sizeof(int));
    memset((void *)attrList, 0, sizeof(attrList));
    for(int i = 0;  i < relEntries[relIndex].attrCount ; i++){
        attrList[i] = 0;
    }
    string relString(relEntries[relIndex].relName);
    int start = relToAttrIndex[relString]; // get the offset of the first attr in this relation
    for(int i = 0;  i < relEntries[relIndex].attrCount ; i++){
        attrList[i] = start + i;
    }
    // 初始化关系节点的属性列表与长度
    relNode->SetUpNode(attrList, relEntries[relIndex].attrCount);
    free(attrList);

    // 获取该关系表拥有的索引数
    int numConds;
    CountNumConditions(relIndex, numConds);


    // 创建连接节点
    QL_NodeJoin *joinNode = new QL_NodeJoin(*this, *currNode, *relNode);
    if((rc = joinNode->SetUpNode(numConds)))
        return (rc);
    topNode = joinNode;

    for(int i = 0; i < nConds; i++){
        if(conditionToRel[i] == relIndex){
            bool added = false;
            if(condptr[i].op == EQ_OP && !condptr[i].bRhsIsAttr && useIndex == false){
                int index = 0;
                if((rc = GetAttrCatEntryPos(condptr[i].lhsAttr, index) ))
                    return (rc);
                if((attrEntries[index].indexNo != -1)){
                    if((rc = relNode->UseIndex(index, attrEntries[index].indexNo, condptr[i].rhsValue.data) ))
                        return (rc);
                    added = true;
                    useIndex = true;
                }
            }
            else if(condptr[i].op == EQ_OP && condptr[i].bRhsIsAttr && useIndex == false &&
                    !relNode->useIndex ) {
                int index1, index2;
                if((rc = GetAttrCatEntryPos(condptr[i].lhsAttr, index1)))
                    return (rc);
                if((rc = GetAttrCatEntryPos(condptr[i].rhsAttr, index2)))
                    return (rc);
                int relIdx1, relIdx2;
                AttrToRelIndex(condptr[i].lhsAttr, relIdx1);
                AttrToRelIndex(condptr[i].rhsAttr, relIdx2);
                if(relIdx2 == relIndex && attrEntries[index2].indexNo != -1){
                    if((rc = joinNode->UseIndexJoin(index1, index2, attrEntries[index2].indexNo)))
                        return (rc);
                    added = true;
                    useIndex = true;
                }
                else if(relIdx1 == relIndex && attrEntries[index1].indexNo != -1){
                    if((rc = joinNode->UseIndexJoin(index2, index1, attrEntries[index1].indexNo)))
                        return (rc);
                    added = true;
                    useIndex = true;
                }
            }
            if(! added){
                if((rc = topNode->AddCondition(condptr[i], i)))
                    return (rc);
            }
        }
    }

    return (rc);
}

RC QL_Manager::RecalcCondToRel(QO_Rel* qorels){
    set<int> relsSeen;
    for(int i=0; i < nRels; i++){
        int relIndex = qorels[i].relIdx;
        relsSeen.insert(relIndex);
        for(int j=0; j < nConds; j++){
            if(condptr[j].bRhsIsAttr){
                int index1, index2;
                AttrToRelIndex(condptr[j].lhsAttr, index1);
                AttrToRelIndex(condptr[j].rhsAttr, index2);
                bool found1 = (relsSeen.find(index1) != relsSeen.end());
                bool found2 = (relsSeen.find(index2) != relsSeen.end());
                if(found1 && found2 && (relIndex == index1 || relIndex== index2))
                    conditionToRel[j] = relIndex;
            }
            else{
                int index1;
                AttrToRelIndex(condptr[j].lhsAttr, index1);
                if(index1 == relIndex)
                    conditionToRel[j] = relIndex;
            }
        }
    }
    return (0);
}

RC QL_Manager::AttrToRelIndex(const RelAttr attr, int& relIndex){
    if(attr.relName != NULL){
        string relName(attr.relName);
        relIndex = relToInt[relName];
    }
    else{
        string attrName(attr.attrName);
        set<string> relSet = attrToRel[attrName];
        relIndex = relToInt[*relSet.begin()];
    }

    return (0);
}

RC QL_Manager::JoinRelationWithQO(QL_Node *&topNode, QO_Rel* qorels, QL_Node *currNode, int qoIdx){
    RC rc = 0;
    int relIndex = qorels[qoIdx].relIdx;
    // create new relation node, providing the relation entry
    QL_NodeRel *relNode = new QL_NodeRel(*this, relEntries + relIndex);

    // Set up the list of indices corresponding to attributes in the relation
    int *attrList = (int *)malloc(relEntries[relIndex].attrCount * sizeof(int));
    memset((void *)attrList, 0, sizeof(attrList));
    for(int i = 0;  i < relEntries[relIndex].attrCount ; i++){
        attrList[i] = 0;
    }
    string relString(relEntries[relIndex].relName);
    int start = relToAttrIndex[relString]; // get the offset of the first attr in this relation
    for(int i = 0;  i < relEntries[relIndex].attrCount ; i++){
        attrList[i] = start + i;
    }
    // Set up the relation node by providing the attribute list and # of attributes
    relNode->SetUpNode(attrList, relEntries[relIndex].attrCount);
    free(attrList);

    // 获取该关系表拥有的索引数
    int numConds;
    CountNumConditions(relIndex, numConds);

    // create new join node:
    QL_NodeJoin *joinNode = new QL_NodeJoin(*this, *currNode, *relNode);
    if((rc = joinNode->SetUpNode(numConds))) // provide a max count on # of conditions
        return (rc);                           // to add to this join
    topNode = joinNode;

    if(qorels[qoIdx].indexAttr != -1){
        int condIdx = qorels[qoIdx].indexCond;
        int index = qorels[qoIdx].indexAttr;
        int index1 = 0;
        int index2 = 0;
        GetAttrCatEntryPos(condptr[condIdx].lhsAttr, index1);
        if(condptr[condIdx].bRhsIsAttr)
            GetAttrCatEntryPos(condptr[condIdx].rhsAttr, index2);
        int otherAttr;
        if(index1 == index)
            otherAttr = index2;
        else
            otherAttr = index1;
        if((attrEntries[index].indexNo != -1) && !condptr[condIdx].bRhsIsAttr){ // add only if there is an index on this attribute
            //cout << "adding index join on attr " << index;
            if((rc = relNode->UseIndex(index, attrEntries[index].indexNo, condptr[condIdx].rhsValue.data) ))
                return (rc);
        }
        else if((attrEntries[index].indexNo != -1) && condptr[condIdx].bRhsIsAttr){
            //cout << "adding, lhs attr: " << otherAttr << ", rhsATtr: " << index << endl;
            if((rc = joinNode->UseIndexJoin(otherAttr, index, attrEntries[index].indexNo)))
                return (rc);
        }
    }
    for(int i = 0 ; i < nConds; i++){
        if(conditionToRel[i] == relIndex){
            if((rc = topNode->AddCondition(condptr[i], i) ))
                return (rc);
        }
    }
    return (0);
}

/**
 * 建立查询树的第一个节点
 */
RC QL_Manager::SetUpFirstNode(QL_Node *&topNode){
    RC rc = 0;
    bool useSelNode = false;
    bool useIndex = false;
    int relIndex = 0; // 从关系列表中第一个关系表开始

    QL_NodeRel *relNode = new QL_NodeRel(*this, relEntries);
    topNode = relNode;

    // 初始化该关系表的属性列表
    int *attrList = (int *)malloc(relEntries[relIndex].attrCount * sizeof(int));
    memset((void *)attrList, 0, sizeof(attrList));
    for(int i = 0;  i < relEntries[relIndex].attrCount ; i++){
        attrList[i] = 0;
    }
    string relString(relEntries[relIndex].relName);
    int start = relToAttrIndex[relString];
    for(int i = 0;  i < relEntries[relIndex].attrCount ; i++){
        attrList[i] = start + i;
    }
    // 初始化该关系节点的属性列表和属性个数变量
    relNode->SetUpNode(attrList, relEntries[relIndex].attrCount);
    free(attrList);

    // 计算一个关系表拥有的索引数
    int numConds;
    CountNumConditions(0, numConds);

    for(int i = 0 ; i < nConds; i++){
        if(conditionToRel[i] == 0){
            bool added = false;
            if(condptr[i].op == EQ_OP && !condptr[i].bRhsIsAttr && useIndex == false && isUpdate == false){
                int index = 0;
                if((rc = GetAttrCatEntryPos(condptr[i].lhsAttr, index) ))
                    return (rc);
                if((attrEntries[index].indexNo != -1)){
                    if((rc = relNode->UseIndex(index, attrEntries[index].indexNo, condptr[i].rhsValue.data)))
                        return (rc);
                    added = true;
                    useIndex = true;
                }
            }
            if(!added && !useSelNode){
                QL_NodeSel *selNode = new QL_NodeSel(*this, *relNode);
                if((rc = selNode->SetUpNode(numConds) ))
                    return (rc);
                topNode = selNode;
                useSelNode = true;
            }
            if(!added){
                if((rc = topNode->AddCondition(condptr[i], i) ))
                    return (rc);
            }
        }
    }
    return (0);
}
/**
 * 使用查询优化器建立查询树根节点
 */
RC QL_Manager::SetUpFirstNodeWithQO(QL_Node *&topNode, QO_Rel* qorels){
    RC rc = 0;
    int relIndex = qorels[0].relIdx;

    QL_NodeRel *relNode = new QL_NodeRel(*this, relEntries + relIndex);
    topNode = relNode;

    int *attrList = (int *)malloc(relEntries[relIndex].attrCount * sizeof(int));
    memset((void *)attrList, 0, sizeof(attrList));
    for(int i = 0;  i < relEntries[relIndex].attrCount ; i++){
        attrList[i] = 0;
    }
    string relString(relEntries[relIndex].relName);
    int start = relToAttrIndex[relString];
    for(int i = 0;  i < relEntries[relIndex].attrCount ; i++){
        attrList[i] = start + i;
    }
    relNode->SetUpNode(attrList, relEntries[relIndex].attrCount);
    free(attrList);

    int numConds;
    CountNumConditions(relIndex, numConds);

    if(qorels[0].indexAttr != -1){
        int index = qorels[0].indexAttr;
        int condIdx = qorels[0].indexCond;
        if((attrEntries[index].indexNo != -1)){
            if((rc = relNode->UseIndex(index, attrEntries[index].indexNo, condptr[condIdx].rhsValue.data) ))
                return (rc);
        }
    }
    if(numConds > 0){
        QL_NodeSel *selNode = new QL_NodeSel(*this, *relNode);
        if((rc = selNode->SetUpNode(numConds) ))
            return (rc);
        topNode = selNode;
    }
    for(int i = 0 ; i < nConds; i++){
        if(conditionToRel[i] == relIndex){
            if((rc = topNode->AddCondition(condptr[i], i) ))
                return (rc);
        }
    }
    return (0);
}

/**
 * 计算某一索引表示的关系表含有的条件数量
 */
RC QL_Manager::CountNumConditions(int relIndex, int &numConds){
    RC rc = 0;
    numConds = 0;
    for(int i = 0; i < nConds; i++ ){
        if( conditionToRel[i] == relIndex)
            numConds++;
    }
    return (0);
}

/**
 * 获取一个指向由参数中attr变量表示的属性的指针
 */
RC QL_Manager::GetAttrCatEntry(const RelAttr attr, AttrCatEntry *&entry){
    RC rc = 0;
    int index = 0;
    if((rc = GetAttrCatEntryPos(attr, index)))
        return (rc);
    entry = attrEntries + index;
    return (0);
}

/**
 * 获取某属性在属性列表中的下标
 */
RC QL_Manager::GetAttrCatEntryPos(const RelAttr attr, int &index){
    // 获取关系表名
    string relString;
    if(attr.relName != NULL){
        string relStringTemp(attr.relName);
        relString = relStringTemp;
    }
    else{
        string attrString(attr.attrName);
        set<string> relNames = attrToRel[attrString];
        set<string>::iterator it = relNames.begin();
        relString = *it;
    }
    int relNum = relToInt[relString];
    int numAttrs = relEntries[relNum].attrCount; // 关系表的属性个数
    int slot = relToAttrIndex[relString]; // 该关系第一个属性在属性列表中的下标
    for(int i=0; i < numAttrs; i++){
        // 寻找属性名匹配的属性在属性列表中的下标
        int comp = strncmp(attr.attrName, attrEntries[slot + i].attrName, strlen(attr.attrName));
        if(comp == 0){
            index = slot + i;
            return (0);
        }
    }
    return (QL_ATTRNOTFOUND);
}

/**
 * 检查select操作涉及的属性是否合法 如果全部合法返回0
 */
RC QL_Manager::ParseSelectAttrs(int nSelAttrs, const RelAttr selAttrs[]){
    // 如果是select * 判断为合法
    if((nSelAttrs == 1 && strncmp(selAttrs[0].attrName, "*", strlen(selAttrs[0].attrName)) == 0))
        return (0);
    //对所有属性逐一判断是否合法
    for(int i=0; i < nSelAttrs; i++){
        // 检查属性是否合法
        if(!IsValidAttr(selAttrs[i]))
            return (QL_ATTRNOTFOUND);
    }
    return (0);
}

/**
 * 检查语句中条件是否合法（在delete select update中都能调用） 若全部合法则返回0
 */
RC QL_Manager::ParseConditions(int nConditions, const Condition conditions[]){
    RC rc = 0;
    for(int i=0; i < nConditions; i++){
        //遍历检查所有条件
        //先检查左侧属性是否合法
        if(!IsValidAttr(conditions[i].lhsAttr)){
            return (QL_ATTRNOTFOUND);
        }
        // 如果右侧值是常量
        if(!conditions[i].bRhsIsAttr){
            // 判断数据类型是否相同
            AttrCatEntry *entry;
            if((rc = GetAttrCatEntry(conditions[i].lhsAttr, entry)))
                return (rc);
            if(entry->attrType != conditions[i].rhsValue.type)
                return (QL_BADCOND);
            // 条件合法 将条件加入条件列表
            string relString(entry->relName);
            int relNum = relToInt[relString];
            conditionToRel.insert({i, relNum});
        }
        else{
            // 如果右侧值是属性
            // 判断属性是否合法
            if(!IsValidAttr(conditions[i].rhsAttr))
                return (QL_ATTRNOTFOUND);
            AttrCatEntry *entry1;
            AttrCatEntry *entry2;
            // 检查类型是否一致
            if((rc = GetAttrCatEntry(conditions[i].lhsAttr,entry1)) || (rc = GetAttrCatEntry(conditions[i].rhsAttr, entry2)))
                return (rc);
            if(entry1->attrType != entry2->attrType)
                return (QL_BADCOND);
            // 条件合法 将条件加入条件列表
            string relString1(entry1->relName);
            string relString2(entry2->relName);
            int relNum1 = relToInt[relString1];
            int relNum2 = relToInt[relString2];
            conditionToRel.insert({i, max(relNum1, relNum2)});
        }
    }
    return (0);
}


/**
 * 利用Set来检查select操作涉及的关系表名是否有重复
 */
RC QL_Manager::ParseRelNoDup(int nRelations, const char * const relations[]){
    set<string> relationSet;
    for(int i = 0; i < nRelations; i++){
        string relString(relations[i]);
        bool exists = (relationSet.find(relString) != relationSet.end());
        if(exists)
            return (QL_DUPRELATION);
        relationSet.insert(relString);
    }
    return (0);
}

/**
 * 执行插入语句
 */
RC QL_Manager::Insert(const char *relName, int nValues, const Value values[]){
    int i;
    RC rc = 0;

    cout << "Insert\n";

    cout << "   relName = " << relName << "\n";
    cout << "   nValues = " << nValues << "\n";
    for (i = 0; i < nValues; i++)
        cout << "   values[" << i << "]:" << values[i] << "\n";

    //重置参数
    Reset();

    // 为定义关系的目录项变量分配空间
    relEntries = (RelCatEntry *)malloc(sizeof(RelCatEntry));
    memset((void*)relEntries, 0, sizeof(*relEntries));
    *relEntries = (RelCatEntry) {"\0", 0, 0, 0, 0, 0, false};
    // 初始化各个参数
    if((rc = SetUpOneRelation(relName))){
        free(relEntries);
        return (rc);
    }
    // 检查输入的属性数量是否和该关系表所需的属性数量匹配
    if(relEntries->attrCount != nValues){
        free(relEntries);
        return (QL_BADINSERT);
    }

    // 为定义属性的目录项变量分配空间
    attrEntries = (AttrCatEntry *)malloc(relEntries->attrCount * sizeof(AttrCatEntry));
    memset((void*)attrEntries, 0, sizeof(*attrEntries));
    for(int i= 0 ; i < relEntries->attrCount; i++){
        *(attrEntries+i) = (AttrCatEntry) {"\0", "\0", 0, INT, 0, 0, 0, 0, FLT_MIN, FLT_MAX};
    }
    // 初始化各个参数
    if((rc = smm.GetAttrForRel(relEntries, attrEntries, attrToRel))){
        free(relEntries);
        free(attrEntries);
        return (rc);
    }

    // 检查属性和值类别是否匹配
    bool badFormat = false;
    for(int i=0; i < nValues; i++){
        if((values[i].type != attrEntries[i].attrType))
            badFormat = true;
        // 确保字符串参数长度小于最大值
        if(attrEntries[i].attrType == STRING && (strlen((char *) values[i].data) > attrEntries[i].attrLength))
            badFormat = true;
    }
    if(badFormat){
        free(relEntries);
        free(attrEntries);
        return (QL_BADINSERT);
    }
    // 执行插入操作
    rc = InsertIntoRelation(relName, relEntries->tupleLength, nValues, values);

    free(relEntries);
    free(attrEntries);

    return (rc);
}

/**
 * Given a relation name, the tuple length, and the values for the attributes, inserts this
 * tuple into the relation.
 * 向关系表内插入元组 参数是关系表名、元组长度、属性个数和属性数组
 */
RC QL_Manager::InsertIntoRelation(const char *relName, int tupleLength, int nValues, const Value values[]){
    RC rc = 0;

    // 创建打印信息所需的变量
    DataAttrInfo * printAttributes = (DataAttrInfo *)malloc(relEntries->attrCount* sizeof(DataAttrInfo));
    if((rc = SetUpPrinterInsert(printAttributes)))
        return (rc);
    Printer printer(printAttributes, relEntries->attrCount);
    printer.PrintHeader(cout);

    // 打开对应文件
    RM_FileHandle relFH;
    if((rc = rmm.OpenFile(relName, relFH))){
        return (rc);
    }

    // 创建记录 将属性数组的值放入recbuf所指向的空间
    char *recbuf = (char *)malloc(tupleLength);
    CreateRecord(recbuf, attrEntries, nValues, values);

    // 向关系表中插入
    RID recRID;
    if((rc = relFH.InsertRec(recbuf, recRID))){
        free(recbuf);
        return (rc);
    }
    printer.Print(cout, recbuf);

    // 更新相关的索引文件
    if((rc = InsertIntoIndex(recbuf, recRID))){
        free(recbuf);
        return (rc);
    }

    printer.PrintFooter(cout);
    free(recbuf);
    free(printAttributes);
    rc = rmm.CloseFile(relFH);
    return (rc);

}

/**
 * 遍历所有属性，如果某属性有索引的话则将新记录更新至索引
 * insert时调用
 */
RC QL_Manager::InsertIntoIndex(char *recbuf, RID recRID){
    RC rc = 0;
    for(int i = 0; i < relEntries->attrCount; i++){
        AttrCatEntry aEntry = attrEntries[i];
        if(aEntry.indexNo != -1){
            IX_IndexHandle ih;
            if((rc = ixm.OpenIndex(relEntries->relName, aEntry.indexNo, ih)))
                return (rc);
            if((rc = ih.InsertEntry((void *)(recbuf + aEntry.offset), recRID)))
                return (rc);
            if((rc = ixm.CloseIndex(ih)))
                return (rc);
        }
    }
    return (0);
}

/**
 * 给定指向记录buffer的指针，将属性数组中的值复制到对应位置
 */
RC QL_Manager::CreateRecord(char *recbuf, AttrCatEntry *aEntries, int nValues, const Value values[]){
    for(int i = 0; i < nValues; i++){
        AttrCatEntry aEntry = aEntries[i];
        memcpy(recbuf + aEntry.offset, (char *)values[i].data, aEntry.attrLength);
    }
    return (0);
}

/**
 * 根据给定表名初始化relEntries, attrEntires等变量
 * 在insert和delete中使用
 */
RC QL_Manager::SetUpOneRelation(const char *relName){
    RC rc = 0;
    RelCatEntry *rEntry;
    RM_Record relRec;
    if((rc = smm.GetRelEntry(relName, relRec, rEntry))){
        return (rc);
    }
    memcpy(relEntries, rEntry, sizeof(RelCatEntry));

    nRels = 1;
    nAttrs = rEntry->attrCount;
    string relString(relName);
    relToInt.insert({relString, 0});
    relToAttrIndex.insert({relString, 0});
    return (0);
}


/**
 * 删除关系表中所有满足条件的记录
 */
RC QL_Manager::Delete(const char *relName, int nConditions, const Condition conditions[]){
    int i;

    cout << "Delete\n";

    cout << "   relName = " << relName << "\n";
    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    RC rc = 0;
    // 重置参数
    Reset();
    // 初始化参数
    condptr = conditions;
    nConds = nConditions;

    // 为定义关系的目录项变量分配空间
    relEntries = (RelCatEntry *)malloc(sizeof(RelCatEntry));
    memset((void*)relEntries, 0, sizeof(*relEntries));
    *relEntries = (RelCatEntry) {"\0", 0, 0, 0, 0, 0, true};
    // 初始化各个参数
    if((rc = SetUpOneRelation(relName))){
        free(relEntries);
        return (rc);
    }

    // 为该关系表中各属性目录项变量分配空间
    attrEntries = (AttrCatEntry *)malloc(relEntries->attrCount * sizeof(AttrCatEntry));
    memset((void*)attrEntries, 0, sizeof(*attrEntries));
    // 初始化属性目录项数组中各个变量
    for(int i= 0 ; i < relEntries->attrCount; i++){
        *(attrEntries+i) = (AttrCatEntry) {"\0", "\0", 0, INT, 0, 0, 0, 0, FLT_MIN, FLT_MAX};
    }
    // 计算各属性值
    if((rc = smm.GetAttrForRel(relEntries, attrEntries, attrToRel))){
        free(relEntries);
        free(attrEntries);
        return (rc);
    }

    // 检查条件是否合法
    if(rc = ParseConditions(nConditions, conditions)){
        free(relEntries);
        free(attrEntries);
        return (rc);
    }

    // 创建查询树节点
    QL_Node *topNode;
    if((rc = SetUpFirstNode(topNode)))
        return (rc);

    // 打印查询计划
    if(bQueryPlans){
        cout << "PRINTING QUERY PLAN" <<endl;
        topNode->PrintNode(0);
    }

    // 执行删除操作
    if((rc = RunDelete(topNode)))
        return (rc);

    // 清除查询树节点
    if(rc = CleanUpNodes(topNode))
        return (rc);

    free(relEntries);
    free(attrEntries);

    return 0;
}

/**
 * 创建属性信息列表，每个元素含有一个IX_IndexHandle引用，用于删除对应索引
 */
RC QL_Manager::SetUpRun(Attr* attributes, RM_FileHandle &relFH){
    RC rc = 0;
    for(int i=0; i < relEntries->attrCount; i++){
        memset((void*)&attributes[i], 0, sizeof(attributes[i]));
        IX_IndexHandle ih;
        attributes[i] = (Attr) {0, 0, 0, 0, ih, NULL};
    }
    if((rc = smm.PrepareAttr(relEntries, attributes)))
        return (rc);
    if((rc = rmm.OpenFile(relEntries->relName, relFH)))
        return (rc);
    return (0);
}

/**
 * 清理在SetUpRun()函数中创建的属性数组
 */
RC QL_Manager::CleanUpRun(Attr* attributes, RM_FileHandle &relFH){
    RC rc = 0;
    if( (rc = rmm.CloseFile(relFH)))
        return (rc);
    if((rc = smm.CleanUpAttr(attributes, relEntries->attrCount)))
        return (rc);
    return (0);
}

/**
 * 给定查询树头节点，删除满足条件的记录并更新索引文件
 */
RC QL_Manager::RunDelete(QL_Node *topNode){
    RC rc = 0;
    // 获取该关系表的属性列表
    int finalTupLength, attrListSize;
    topNode->GetTupleLength(finalTupLength);
    int *attrList;
    if((rc = topNode->GetAttrList(attrList, attrListSize)))
        return (rc);
    DataAttrInfo * printAttributes = (DataAttrInfo *)malloc(attrListSize* sizeof(DataAttrInfo));
    if((rc = SetUpPrinter(topNode, printAttributes)))
        return (rc);
    Printer printer(printAttributes, attrListSize);
    printer.PrintHeader(cout);

    // 创建RM_FileHandle变量和Attr数组
    RM_FileHandle relFH;
    Attr* attributes = (Attr *)malloc(sizeof(Attr)*relEntries->attrCount);
    // 初始化上述变量 其中Attr数组中每个元素都持有一个IX_IndexHandles变量，用于更新索引文件
    if((rc = SetUpRun(attributes, relFH))){
        smm.CleanUpAttr(attributes, relEntries->attrCount);
        return (rc);
    }

    // 获取记录
    if((rc = topNode->OpenIt() ))
        return (rc);

    RM_Record rec;
    RID rid;
    while(true){
        if((rc = topNode->GetNextRec(rec))){
            if (rc == QL_EOI){
                // 遍历结束
                break;
            }
            return (rc);
        }
        char *pData;
        if((rc = rec.GetRid(rid)) || (rc = rec.GetData(pData)) )
            return (rc);
        // 打印要删除的属性信息
        printer.Print(cout, pData);
        // 删除记录
        if((rc = relFH.DeleteRec(rid)))
            return (rc);

        // 从索引文件中删除
        for(int i=0; i < relEntries->attrCount ; i++){
            if(attributes[i].indexNo != -1){
                if((rc = attributes[i].ih.DeleteEntry((void *)(pData + attributes[i].offset), rid)))
                    return (rc);
            }
        }
    }

    if((rc = topNode->CloseIt()))
        return (rc);

    // 销毁变量
    if((rc = CleanUpRun(attributes, relFH)))
        return (rc);
    printer.PrintFooter(cout);
    free(printAttributes);

    return (0);
}


/**
 * 从根节点递归删除查询树
 */
RC QL_Manager::CleanUpNodes(QL_Node *topNode){
    RC rc = 0;
    if((rc = topNode->DeleteNodes()))
        return (rc);
    delete topNode;
    return (0);
}

/**
 * 检查更新操作中新值是否合法
 */
RC QL_Manager::CheckUpdateAttrs(const RelAttr &updAttr,
                                const int bIsValue,
                                const RelAttr &rhsRelAttr,
                                const Value &rhsValue){
    RC rc = 0;
    // 检查待赋值属性是否合法
    if(!IsValidAttr(updAttr))
        return (QL_ATTRNOTFOUND);
    if(bIsValue){
        // 右侧值为常量
        AttrCatEntry *entry;
        if((rc = GetAttrCatEntry(updAttr, entry)))
            return (rc);
        // 检查左右侧值类型是否匹配
        if(entry->attrType != rhsValue.type)
            return (QL_BADUPDATE);
        // 对字符串长度进行检测
        if(entry->attrType == STRING){
            int newValueSize = strlen((char *)rhsValue.data);
            if(newValueSize > entry->attrLength)
                return (QL_BADUPDATE);
        }
    }
    else{
        // 右侧为属性
        // 检测属性是否合法
        if(!IsValidAttr(rhsRelAttr))
            return (QL_ATTRNOTFOUND);
        AttrCatEntry *entry1;
        AttrCatEntry *entry2;
        if((rc = GetAttrCatEntry(updAttr, entry1)) || (rc = GetAttrCatEntry(rhsRelAttr, entry2)))
            return (rc);
        if(entry1->attrType != entry2->attrType)
            return (QL_BADUPDATE);
        if(entry1->attrType == STRING){
            if(entry2->attrLength > entry1->attrLength)
                return (QL_BADUPDATE);
        }
    }

    return (rc);
}

/**
 * 执行更新操作
 */
RC QL_Manager::RunUpdate(QL_Node *topNode, const RelAttr &updAttr,
                         const int bIsValue,
                         const RelAttr &rhsRelAttr,
                         const Value &rhsValue){
    RC rc = 0;
    // 获取关系中的属性列表
    int finalTupLength, attrListSize;
    topNode->GetTupleLength(finalTupLength);
    int *attrList;
    if((rc = topNode->GetAttrList(attrList, attrListSize)))
        return (rc);
    DataAttrInfo * attributes = (DataAttrInfo *)malloc(attrListSize* sizeof(DataAttrInfo));
    // 初始化Printer
    if((rc = SetUpPrinter(topNode, attributes)))
        return (rc);
    Printer printer(attributes, attrListSize);
    printer.PrintHeader(cout);


    // 打开文件
    RM_FileHandle relFH;
    if((rc = rmm.OpenFile(relEntries->relName, relFH)))
        return (rc);

    int index1, index2;
    if((rc = GetAttrCatEntryPos(updAttr, index1)))
        return (rc);
    if(!bIsValue){
        if((rc = GetAttrCatEntryPos(rhsRelAttr, index2)))
            return (rc);
    }

    // 如果要更新的属性上有索引 则打开索引
    IX_IndexHandle ih;
    if((attrEntries[index1].indexNo != -1)){
        if((rc = ixm.OpenIndex(relEntries->relName, attrEntries[index1].indexNo, ih)))
            return (rc);
    }

    // 查找到左右满足条件的记录
    if((rc = topNode->OpenIt() ))
        return (rc);
    RM_Record rec;
    RID rid;
    while(true){
        // 遍历所有待更新节点
        if((rc = topNode->GetNextRec(rec))){
            if (rc == QL_EOI){
                break;
            }
            return (rc);
        }
        char *pData;
        if((rc = rec.GetRid(rid)) || (rc = rec.GetData(pData)) )
            return (rc);
        // 如果有索引则删除该值的索引
        if(attrEntries[index1].indexNo != -1){
            if((ih.DeleteEntry(pData + attrEntries[index1].offset, rid)))
                return (rc);
        }

        // 将新值赋值给该属性
        if(bIsValue){
            // 常量的情况
            if(attrEntries[index1].attrType == STRING){
                int valueLength = strlen((char *)rhsValue.data);
                if(attrEntries[index1].attrLength <= (valueLength + 1) )
                    memcpy(pData + attrEntries[index1].offset, (char *)rhsValue.data, attrEntries[index1].attrLength);
                else
                    memcpy(pData + attrEntries[index1].offset, (char *)rhsValue.data, valueLength + 1);
            }
            else
                memcpy(pData + attrEntries[index1].offset, (char *)rhsValue.data, attrEntries[index1].attrLength);
        }
        else{
            // 属性的情况
            if(attrEntries[index2].attrLength >= attrEntries[index1].attrLength)
                memcpy(pData + attrEntries[index1].offset, pData + attrEntries[index2].offset, attrEntries[index1].attrLength);
            else{
                memcpy(pData + attrEntries[index1].offset, pData + attrEntries[index2].offset, attrEntries[index2].attrLength);
                pData[attrEntries[index1].offset + attrEntries[index2].attrLength] = '\0';
            }
        }

        // 将记录更新至文件
        if((rc = relFH.UpdateRec(rec)))
            return (rc);
        printer.Print(cout, pData);

        // 如果该属性有索引则更新索引
        if(attrEntries[index1].indexNo != -1){
            if((ih.InsertEntry(pData + attrEntries[index1].offset, rid)))
                return (rc);
        }

    }
    if((rc = topNode->CloseIt()))
        return (rc);

    // 关闭文件与索引文件
    if((attrEntries[index1].indexNo != -1)){
        if((rc = ixm.CloseIndex(ih)))
            return (rc);
    }
    if((rc = rmm.CloseFile(relFH)))
        return (rc);

    printer.PrintFooter(cout);
    free(attributes);

    return (0);
}

/**
 * 更新满足条件的记录
 */
RC QL_Manager::Update(const char *relName,
                      const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue,
                      int nConditions, const Condition conditions[])
{
    int i;

    cout << "Update\n";

    cout << "   relName = " << relName << "\n";
    cout << "   updAttr:" << updAttr << "\n";
    if (bIsValue)
        cout << "   rhs is value: " << rhsValue << "\n";
    else
        cout << "   rhs is attribute: " << rhsRelAttr << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    RC rc = 0;
    // 重置参数
    Reset();
    condptr = conditions;
    nConds = nConditions;
    isUpdate = true;

    // 为定义关系的目录项变量分配空间
    relEntries = (RelCatEntry *)malloc(sizeof(RelCatEntry));
    memset((void*)relEntries, 0, sizeof(*relEntries));
    *relEntries = (RelCatEntry) {"\0", 0, 0, 0, 0, 0, true};
    // 初始化相关变量
    if((rc = SetUpOneRelation(relName))){
        free(relEntries);
        return (rc);
    }

    // 为定义属性的目录项数组分配空间
    attrEntries = (AttrCatEntry *)malloc(relEntries->attrCount * sizeof(AttrCatEntry));
    memset((void*)attrEntries, 0, sizeof(*attrEntries));
    for(int i= 0 ; i < relEntries->attrCount; i++){
        *(attrEntries+i) = (AttrCatEntry) {"\0", "\0", 0, INT, 0, 0, 0, 0, FLT_MIN, FLT_MAX};
    }
    // 初始化相关变量
    if((rc = smm.GetAttrForRel(relEntries, attrEntries, attrToRel))){
        free(relEntries);
        free(attrEntries);
        return (rc);
    }

    // 检查条件是否合法
    if(rc = ParseConditions(nConditions, conditions)){
        free(relEntries);
        free(attrEntries);
        return (rc);
    }

    // 检查待更新属性和值是否合法
    if(rc = CheckUpdateAttrs(updAttr, bIsValue, rhsRelAttr, rhsValue)){
        free(relEntries);
        free(attrEntries);
        return (rc);
    }

    // 创建查询树
    QL_Node *topNode;
    if((rc = SetUpFirstNode(topNode)))
        return (rc);

    // 输出查询计划
    if(bQueryPlans){
        cout << "PRINTING QUERY PLAN" <<endl;
        topNode->PrintNode(0);
    }

    // 执行更新操作
    if((rc = RunUpdate(topNode, updAttr, bIsValue, rhsRelAttr, rhsValue)))
        return (rc);

    // 销毁查询树
    if(rc = CleanUpNodes(topNode))
        return (rc);

    free(relEntries);
    free(attrEntries);

    return 0;
}


/**
 * 初始化Printer
 */
RC QL_Manager::SetUpPrinter(QL_Node *topNode, DataAttrInfo *attributes){
    RC rc = 0;
    // 从节点获取属性列表
    int *attrList;
    int attrListSize;
    if((rc = topNode->GetAttrList(attrList, attrListSize)))
        return (rc);

    for(int i=0; i < attrListSize; i++){
        int index = attrList[i];
        memcpy(attributes[i].relName, attrEntries[index].relName, MAXNAME + 1);
        memcpy(attributes[i].attrName, attrEntries[index].attrName, MAXNAME + 1);
        attributes[i].attrType = attrEntries[index].attrType;
        attributes[i].attrLength = attrEntries[index].attrLength;
        attributes[i].indexNo = attrEntries[index].indexNo;
        int offset, length;
        if((rc = topNode->IndexToOffset(index, offset, length)))
            return (rc);
        attributes[i].offset = offset;
    }

    return (0);
}

/**
 * 为insert操作初始化Printer
 */
RC QL_Manager::SetUpPrinterInsert(DataAttrInfo *attributes){
    RC rc = 0;
    for(int i=0; i < relEntries->attrCount ; i++){
        memcpy(attributes[i].relName, attrEntries[i].relName, MAXNAME +1);
        memcpy(attributes[i].attrName, attrEntries[i].attrName, MAXNAME + 1);
        attributes[i].attrType = attrEntries[i].attrType;
        attributes[i].attrLength = attrEntries[i].attrLength;
        attributes[i].indexNo = attrEntries[i].indexNo;
        attributes[i].offset = attrEntries[i].offset;
    }
    return (0);
}

