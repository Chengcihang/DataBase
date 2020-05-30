//
//  SM_Manager.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/27.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef SM_Manager_h
#define SM_Manager_h

#include "sm_catalog.h"
#include "../RM/rm_manager.h"


//数据管理
class SM_Manager {
    //友元类
    friend class QL_Manager;
    //常量
    static const int NO_INDEXES = -1;
    static const PageNum INVALID_PAGE = -1;
    static const SlotNum INVALID_SLOT = -1;
    
public:
    //构造函数
    SM_Manager(IX_Manager &ixm,
               RM_Manager &rmm);
    
    //析构函数
    ~SM_Manager();

    //打开数据库
    RC OpenDb(const char *dbName);
    //关闭数据库
    RC CloseDb();

    //创建数据表
    RC CreateTable(const char *relName,
                   int attrCount,
                   AttrInfo *attributes);
    //销毁数据表
    RC DropTable(const char *relName);
    
    //创建索引
    RC CreateIndex(const char *relName,
                   const char *attrName);
    //销毁索引
    RC DropIndex(const char *relName,
                 const char *attrName);
    
    //加载
    RC Load(const char *relName,
            const char *fileName);
    
    //打印数据库的关系
    RC Help();
    //打印模式
    RC Help(const char *relName);
    //打印内容
    RC Print(const char *relName);

    //将指定参数设置为给定值
    RC Set(const char *paramName,
           const char *value);
    
    //打开并加载文件
    RC OpenAndLoadFile(RM_FileHandle &relFH,
                       const char *fileName,
                       Attr* attributes,
                       int attrCount,
                       int recLength);
    
private:
    //是否打印索引
    bool printIndex;
    //是否使用查询优化器
    bool useQO;
    //是否计算统计信息
    bool calcStats;
    //是否打印页面统计信息
    bool printPageStats;
    
    //索引文件管理器
    IX_Manager &ixManager;
    //记录文件管理器
    RM_Manager &rmManager;

    //relcat记录文件接口
    RM_FileHandle relcatFH;
    //attrcat记录文件接口
    RM_FileHandle attrcatFH;
    
    //判断给定属性是否具有有效/匹配的类型和长度
    bool isValidAttrType(AttrInfo attribute);
    
    //将有关指定relName关系的条目插入relcat
    RC InsertRelCat(const char *relName,
                    int attrCount,
                    int recSize);

    //将有关指定属性的条目插入attrcat
    RC InsertAttrCat(const char *relName,
                     AttrInfo attr,
                     int offset,
                     int attrNum);
    
    //检索与关系条目关联的记录和数据
    RC GetRelEntry(const char *relName,
                   RM_Record &relRec,
                   RelCatEntry *&entry);

    //查找与特定属性关联的条目
    RC FindAttr(const char *relName,
                const char *attrName,
                RM_Record &attrRec,
                AttrCatEntry *&entry);
  
    //从文件设置用于数据属性信息的打印，打印relcat和属性
    RC SetUpPrint(RelCatEntry* rEntry,
                  DataAttrInfo *attributes);
    RC SetUpRelCatAttributes(DataAttrInfo *attributes);
    RC SetUpAttrCatAttributes(DataAttrInfo *attributes);

    //准备Attribute数组，帮助加载
    RC PrepareAttr(RelCatEntry *rEntry,
                   Attr* attributes);

    //给定一个RelCatEntry，它将使用有关其所有属性的信息填充aEntry并更新属性到关系的映射
    RC GetAttrForRel(RelCatEntry *relEntry,
                     AttrCatEntry *aEntry,
                     std::map<std::string,
                     std::set<std::string> > &attrToRel);
    
    //检索与给定关系列表关联的所有relCatEntries
    RC GetAllRels(RelCatEntry *relEntries,
                  int nRelations,
                  const char * const relations[],
                  int &attrCount,
                  std::map<std::string,
                  int> &relToInt);

    //打开文件并加载
    RC OpenAndLoadFile(RM_FileHandle &relFH,
                       const char *fileName,
                       Attr* attributes,
                       int attrCount,
                       int recLength,
                       int &loadedRecs);
    
    //加载后清理Attribute数组
    RC CleanUpAttr(Attr* attributes,
                   int attrCount);
    
    //转换字符串到浮点数
    float ConvertStrToFloat(char *string);
    
    //打印统计信息
    RC PrintStats(const char *relName);
    RC PrintPageStats();
    //计算统计信息
    RC CalcStats(const char *relName);
    //重置页面统计信息
    RC ResetPageStats();
};


#endif /* SM_Manager_h */
