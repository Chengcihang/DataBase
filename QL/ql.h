
#ifndef QL_H
#define QL_H

#include <string>
#include <set>
#include <map>
#include <stdlib.h>
#include <string.h>
#include "../redbase.h"
#include "../PARSER/parser.h"
#include "../IX/ix.h"
#include "ql_node.h"
#include "../RM/rm_manager.h"
#include "../SM/printer.h"


typedef struct QO_Rel{
    int relIdx;
    int indexAttr;
    int indexCond;
} QO_Rel;

// QL_Manager类
class QL_Manager {
    friend class QL_Node;
    friend class Node_Rel;
    friend class QL_NodeJoin;
    friend class QL_NodeRel;
    friend class QL_NodeSel;
    friend class QL_NodeProj;
    friend class QO_Manager;
public:
    QL_Manager (SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm);
    ~QL_Manager();

    RC Select  (int nSelAttrs,                   // select后的属性个数
                const RelAttr selAttrs[],        // 需要select的属性数组 长度为nSelAttrs
                int   nRelations,                // 数据表的个数
                const char * const relations[],  // 关系表数组（可能有多个关系表）
                int   nConditions,               // 条件个数
                const Condition conditions[]);   // 条件数组

    RC Insert  (const char *relName,             // 需插入的关系表名
                int   nValues,                   // 要插入的值个数
                const Value values[]);           // 值数组

    RC Delete  (const char *relName,             // 需要删除元组的关系表名
                int   nConditions,               // 条件个数
                const Condition conditions[]);   // 条件数组

    RC Update  (const char *relName,             // 需要更新的关系表名
                const RelAttr &updAttr,          // 需要更新的属性
                const int bIsValue,              // =1时表示新属性值为rhsValue重的常量 =0表示新属性值应从rhsRelAttr中获得
                const RelAttr &rhsRelAttr,       // 右侧属性
                const Value &rhsValue,           // 右侧值
                int   nConditions,               // 条件个数
                const Condition conditions[]);   // 条件数组

private:
    // 初始化各参数 准备执行下一条命令
    RC Reset();
    // 检查查询操作的属性是否合法
    bool IsValidAttr(const RelAttr attr);
    // 判断操作涉及的关系表名存在于数据库中且关系表名不重复
    RC ParseRelNoDup(int nRelations, const char * const relations[]);
    // 解析select操作涉及的属性 判断是否合法
    RC ParseSelectAttrs(int nSelAttrs, const RelAttr selAttrs[]);
    //获取指向属性列表attrEntries中参数attr属性的指针
    RC GetAttrCatEntry(const RelAttr attr, AttrCatEntry *&entry);
    //获取参数中attr属性在属性列表中的下标
    RC GetAttrCatEntryPos(const RelAttr attr, int &index);
    // 解析操作中的条件语句 判断是否合法
    RC ParseConditions(int nConditions, const Condition conditions[]);
    // 根据给定关系表名初始化relEntries, attrEntires等变量
    RC SetUpOneRelation(const char *relName);
    // 检查查update操作涉及的属性是否合法
    RC CheckUpdateAttrs(const RelAttr &updAttr,
                        const int bIsValue,
                        const RelAttr &rhsRelAttr,
                        const Value &rhsValue);
    // 删除查询树
    RC RunDelete(QL_Node *topNode);
    // 执行更新
    RC RunUpdate(QL_Node *topNode, const RelAttr &updAttr,    //属性信息
                 const int bIsValue,                   //标志位
                 const RelAttr &rhsRelAttr,            //属性
                 const Value &rhsValue);               //常量（int, float, string）
    // 执行select操作 参数是头节点
    RC RunSelect(QL_Node *topNode);
    // 将一系列值插入关系表
    RC InsertIntoRelation(const char *relName, int tupleLength, int nValues, const Value values[]);
    // 向关系的索引中插入新纪录
    RC InsertIntoIndex(char *recbuf, RID recRID);
    // 给定指向记录buffer的指针，将属性数组中的值复制到对应位置
    RC CreateRecord(char *recbuf, AttrCatEntry *aEntries, int nValues, const Value values[]);
    // 创建属性信息列表，每个元素含有一个IX_IndexHandle引用，用于删除对应索引
    RC SetUpRun(Attr* attributes, RM_FileHandle &relFH);
    // 清理在SetUpRun()函数中创建的属性数组
    RC CleanUpRun(Attr* attributes, RM_FileHandle &relFH);
    // 清除节点
    RC CleanUpNodes(QL_Node *topNode);
    // 计算索引表示的关系表含有的条件数量
    RC CountNumConditions(int relIndex, int &numConds);
    // 为生成查询树初始化第一个节点
    RC SetUpFirstNode(QL_Node *&topNode);
    // 生成整个查询树并返回头节点
    RC SetUpNodes(QL_Node *&topNode, int nSelAttrs, const RelAttr selAttrs[]);
    //使用查询优化器生成查询树并返回头节点
    RC SetUpNodesWithQO(QL_Node *&topNode, QO_Rel* qorels, int nSelAttrs, const RelAttr selAttrs[]);
    RC SetUpFirstNodeWithQO(QL_Node *&topNode, QO_Rel* qorels);
    RC JoinRelationWithQO(QL_Node *&topNode, QO_Rel* qorels, QL_Node *currNode, int relIndex);
    RC RecalcCondToRel(QO_Rel* qorels);
    RC AttrToRelIndex(const RelAttr attr, int& relIndex);
    // 为特定关系表创建连接节点和关系节点并返回头节点
    RC JoinRelation(QL_Node *&topNode, QL_Node *currNode, int relIndex);
    RC SetUpPrinter(QL_Node *topNode, DataAttrInfo *attributes);
    RC SetUpPrinterInsert(DataAttrInfo *attributes);

    //RM IX SM模块manager的引用
    RM_Manager &rmm;
    IX_Manager &ixm;
    SM_Manager &smm;

    // 存储关系表名向该关系表在relEntries中下标的映射关系的map
    std::map<std::string, int> relToInt;
    // 存储属性名向含有该属性名所有关系表名集合的映射关系的map
    std::map<std::string, std::set<std::string> > attrToRel;
    // 存储关系表名向该关系中第一个关系下标的映射的map
    std::map<std::string, int> relToAttrIndex;
    // maps from condition to the relation it's meant to be grouped with
    std::map<int, int> conditionToRel;

    // 关系列表
    RelCatEntry *relEntries;
    // 属性列表
    AttrCatEntry *attrEntries;

    // 属性、关系、条件数量
    int nAttrs;
    int nRels;
    int nConds;
    // 是否有更新操作（决定是否使用索引）
    bool isUpdate;

    // 指向条件列表的指针
    const Condition *condptr;

};


// QL模块输出错误函数
void QL_PrintError(RC rc);

//错误状态码
#define QL_BADINSERT            (START_QL_WARN + 0) // Bad insert
#define QL_DUPRELATION          (START_QL_WARN + 1) // 关系表重复 Duplicate relation
#define QL_BADSELECTATTR        (START_QL_WARN + 2) // select错误属性 Bad select attribute
#define QL_ATTRNOTFOUND         (START_QL_WARN + 3) // 未找到属性 Attribute not found
#define QL_BADCOND              (START_QL_WARN + 4) // 错误条件 Bad condition
#define QL_BADCALL              (START_QL_WARN + 5) // 错误调用 Bad/invalid call
#define QL_CONDNOTMET           (START_QL_WARN + 6) // 没有满足条件的元组 Condition has not been met
#define QL_BADUPDATE            (START_QL_WARN + 7) // 更新语句错误Bad update statement
#define QL_EOI                  (START_QL_WARN + 8) // 迭代器结束 End of iterator
#define QO_BADCONDITION         (START_QL_WARN + 9)
#define QO_INVALIDBIT           (START_QL_WARN + 10)
#define QL_LASTWARN             QL_EOI

#define QL_INVALIDDB            (START_QL_ERR - 0)
#define QL_ERROR                (START_QL_ERR - 1) // error
#define QL_LASTERROR            QL_ERROR



#endif
