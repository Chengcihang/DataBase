//
//  SM_Manager.cpp
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/26.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#include <cstdio>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>

#include "../redbase.h"
#include "sm.h"
#include "../IX/ix.h"
#include "../RM/rm.h"
#undef max
#include <vector>
#include <string>
#include <set>
#include <cfloat>

#include <cstddef>
#include "statistics.h"
#include "../RM/rm_manager.h"

#include "sys/stat.h"
#include <dirent.h>
#include "sm_manager.h"
#include "sm_scan.h"

using namespace std;

StatisticsManager *pStatisticsMgr;
extern void PF_Statistics();


#pragma mark - 解析int,float或string的字符串形式
//解析int的字符串形式，并在装入过程中移动到记录对象中
bool recInsert_int(char *location,
                   string value,
                   int length) {
    int num;
    istringstream ss(value);
    ss >> num;
    if(ss.fail())
        return false;
    //printf("num: %d \n", num);
    memcpy(location, (char*)&num, length);
    return true;
}

//解析float的字符串形式，并在装入过程中移动到记录对象中
bool recInsert_float(char *location,
                     string value,
                     int length) {
    float num;
    istringstream ss(value);
    ss >> num;
    if(ss.fail())
        return false;
    memcpy(location, (char*)&num, length);
    return true;
}

//解析string的字符串形式，并在装入过程中移动到记录对象中
bool recInsert_string(char *location,
                      string value,
                      int length) {
    if(value.length() >= length){
        memcpy(location, value.c_str(), length);
        return true;
    }
    memcpy(location, value.c_str(), value.length()+1);
    return true;
}


#pragma mark - 构造函数和析构函数
//构造函数
SM_Manager::SM_Manager(IX_Manager &ixm,
                       RM_Manager &rmm): ixManager(ixm), rmManager(rmm) {
    //设置标识位
    printIndex = false;
    useQO = true;
    calcStats = false;
    printPageStats = true;
}

//析构函数
SM_Manager::~SM_Manager() {}


#pragma mark - 数据库操作库
/**
1. 创建数据库
输入参数：数据库名称
*/
//创建数据库
RC SM_Manager::CreateDb(const char *dbName) {
    RC rc = OK_RC;
    int status = mkdir(dbName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    //创建成功
    if (status == 0) {
        printf("创建数据库%s成功 \n",dbName);
    }
    else {
        printf("创建数据库%s失败 \n",dbName);
        rc = 1;
    }
    
    if (chdir(dbName) < 0) {
        //无法进入数据库目录
        printf("chdir error to %s \n", dbName);
        rc = 1;
    }
    else {
        //成功进入后，创建文件relcat和attrcat
        if ((rc = rmManager.CreateFile ("relcat", 1000))) {
            printf("创建数据库 %s 下relcat文件失败 \n", dbName);
        }
        
        if ((rc = rmManager.CreateFile ("attrcat", 1000))) {
            printf("创建数据库 %s 下attrcat文件失败 \n", dbName);
        }
    }
    
    if (chdir("..") < 0) {
        //无法进入数据库目录
        printf("无法返回上一级目录\n");
        rc = 1;
    }
    return rc;
}

/**
2. 销毁数据库
输入参数：数据库名称
*/
//销毁数据库
RC SM_Manager::DropDb(const char *dbName) {
    //库名
    std::string file_path = dbName;
    
    struct stat st;
    if(lstat(file_path.c_str(),&st) == -1) {
        printf("指定数据库%s信息有误 \n",dbName);
        return -1;
    }
    
    //是常规文件
    if(S_ISREG(st.st_mode)) {
        if(unlink(file_path.c_str()) == -1) {
            printf("删除%s文件失败 \n",dbName);
            return -1;
        }
    }
    //是目录
    else if(S_ISDIR(st.st_mode)) {
        if(dbName == "." || dbName == "..") {
            printf("数据库%s名称有误 \n",dbName);
            return -1;
        }
        
        //遍历删除指定数据库下的文件
        DIR* dirp = opendir(file_path.c_str());
        if(!dirp) {
            return -1;
        }
        struct dirent *dir;
        struct stat st;
        //遍历
        while((dir = readdir(dirp)) != NULL) {
            if(strcmp(dir->d_name,".") == 0
                    || strcmp(dir->d_name,"..") == 0)
            {
                continue;
            }
            std::string sub_path = file_path + '/' + dir->d_name;
            if(lstat(sub_path.c_str(),&st) == -1) {
                continue;
            }
            // 如果是普通文件，则unlink
            if(S_ISREG(st.st_mode)) {
                unlink(sub_path.c_str());
            }
            else {
                printf("数据库%s下包含非法文件，无法删除 \n",dbName);
                continue;
            }
        }
        //删除目录（需要空）
        if(rmdir(file_path.c_str()) == -1) {
            closedir(dirp);
            printf("删除数据库%s失败 \n",dbName);
            return -1;
        }
        closedir(dirp);
    }
    return 0;
}

/**
 3. 打开对应于数据库的文件夹并进入该文件夹
 输入参数：数据库名称
 */
RC SM_Manager::OpenDb(const char *dbName) {
    RC rc = 0;
  
    //检查是否为有效的dbName
    if(strlen(dbName) > MAX_DB_NAME){
        return (SM_INVALIDDB);
    }

    //chdir系统调用函数 同cd 用于改变当前工作目录，其参数为Path 目标目录，可以是绝对目录或相对目录。
    if(chdir(dbName) < 0){
        cerr << "Cannot chdir to " << dbName << "\n";
        return (SM_INVALIDDB);
    }

    //打开并保存relcat文件句柄
    if((rc = rmManager.OpenFile("relcat", relcatFH))){
        return (SM_INVALIDDB);
    }
    //打开并保存attrcat文件句柄
    if((rc = rmManager.OpenFile("attrcat", attrcatFH))) {
        return (SM_INVALIDDB);
    }
  
    return (0);
}

/**
 4. 关闭数据库，因此关闭任何打开文件
 输入参数：无
*/
RC SM_Manager::CloseDb() {
    RC rc = 0;
    //关闭relcat文件
    if((rc = rmManager.CloseFile(relcatFH) )){
        return (rc);
    }
    //关闭attrcat文件
    if((rc = rmManager.CloseFile(attrcatFH))){
        return (rc);
    }
    if (chdir("..") < 0) {
        //无法进入数据库目录
        printf("无法返回上一级目录\n");
        rc = 1;
    }
    //正常关闭返回0
    return (0);
}

#pragma mark - 数据库操作 表
/**
 1.创建数据库表
 输入参数：relName表名，attrCount属性数量，attributes属性
 */
RC SM_Manager::CreateTable(const char *relName,
                           int attrCount,
                           AttrInfo *attributes) {
    //输出提示
    cout << "CreateTable\n"
    << "   relName     =" << relName << "\n"
    << "   attrCount   =" << attrCount << "\n";
    
    //输出属性
    for (int i = 0; i < attrCount; i++)
        cout << "   attributes[" << i << "].attrName=" << attributes[i].attrName
        << "   attrType="
        << (attributes[i].attrType == INT ? "INT" :
            attributes[i].attrType == FLOAT ? "FLOAT" : "STRING")
        << "   attrLength=" << attributes[i].attrLength << "\n";
    
    //记录
    RC rc = 0;
    //声明属性容器
    set<string> relAttributes;

    //检查属性数是否合理
    if(attrCount > MAXATTRS || attrCount < 1){
        printf("reaches here\n");
        return (SM_BADREL);
    }
    
    //检查表名是否有效（长度）
    if(strlen(relName) > MAXNAME)
        return (SM_BADRELNAME);

    //检查属性是否规范
    //记录元组长度
    int totalRecSize = 0;
    for(int i = 0; i < attrCount; i++){
        //检查属性名称
        if(strlen(attributes[i].attrName) > MAXNAME)
            return (SM_BADATTR);
        //检查属性类型
        if(! isValidAttrType(attributes[i]))
            return (SM_BADATTR);
        totalRecSize += attributes[i].attrLength;
        //判断是否存在 来决定是否插入
        string attrString(attributes[i].attrName);
        bool exists = (relAttributes.find(attrString) != relAttributes.end());
        if (exists)
            return (SM_BADREL);
        else
            relAttributes.insert(attrString);
    }

    //为此表创建一个文件
    if((rc = rmManager.CreateFile(relName, totalRecSize)))
        return (SM_BADRELNAME);

    //在relcat和attrcat中插入
    //1.对于每个属性，插入attrcat中：
    RID rid;
    int currOffset = 0;
    //1-attrCount 个属性都需要进行的操作
    for(int i = 0; i < attrCount; i++){
        AttrInfo attr = attributes[i];
        //调用辅助函数，向数据库的attcat中插入
        if((rc = InsertAttrCat(relName, attr, currOffset, i)))
            return (rc);
        currOffset += attr.attrLength;
    }
    
    //2.插入到RelCat中
    //调用辅助函数，向数据库的relcat中插入
    if((rc = InsertRelCat(relName, attrCount, totalRecSize)))
        return (rc);

    //强制写回磁盘，确保atttcat和relcat的更改
    if((rc = attrcatFH.ForcePages()) || (rc = relcatFH.ForcePages()))
        return (rc);

    return (0);
}

/**
 2.删除数据库表，并删除其所有索引，删除relcat和attrcat内的相关信息
 输入参数：relName表名
*/
RC SM_Manager::DropTable(const char *relName) {
    //输出提示
    cout << "DropTable\n   relName=" << relName << "\n";
    RC rc = 0;
    //检查输入表名是否为有效名称
    if(strlen(relName) > MAXNAME)
        return (SM_BADRELNAME);
    //销毁此表对应的文件
    if((rc = rmManager.DestroyFile(relName))){
        return (SM_BADRELNAME);
    }

    //检索与关系关联的记录
    RM_Record relRecord;
    RelCatEntry *relcatEntry;
    
    //获取relcat中与输入表名相关的记录和数据
    if((rc = GetRelEntry(relName, relRecord, relcatEntry)))
        return (rc);
    int numAttr = relcatEntry->attrCount;

    //检索其所有属性
    SM_Scan attrScan;
    //打开扫描器
    if((rc = attrScan.OpenScan(attrcatFH, const_cast<char*>(relName))))
        return (rc);
    
    //检索与属性关联的记录
    AttrCatEntry *attrcatEntry;
    RM_Record attrRecord;
    for(int i=0; i < numAttr; i++){
        //获取下一个属性
        if((rc = attrScan.GetNextAttr(attrRecord, attrcatEntry))){
            return (rc);
        }
        //检查是否有索引，若有，则将其删除
        if((attrcatEntry->indexNo != NO_INDEXES)){
            if((rc = DropIndex(relName, attrcatEntry->attrName)))
                return (rc);
        }
        //删除该属性记录
        RID attrRID;
        if((rc = attrRecord.GetRid(attrRID)) || (rc = attrcatFH.DeleteRec(attrRID)))
            return (rc);
    }
    //关闭扫描器
    if((rc = attrScan.CloseScan()))
        return (rc);

    //删除与关系关联的记录
    RID relRID;
    if((rc = relRecord.GetRid(relRID)) || (rc = relcatFH.DeleteRec(relRID)))
        return (rc);

    return (0);
}


#pragma mark - 数据库操作 索引
/**
 1. 创建索引
 输入参数：relName关系名称，attrName属性名称
 */
RC SM_Manager::CreateIndex(const char *relName,
                           const char *attrName) {
    //输出提示
    cout << "CreateIndex\n"
    << "   relName =" << relName << "\n"
    << "   attrName=" << attrName << "\n";
    
    RC rc = 0;
    RM_Record relRecord;
    RelCatEntry *relcatEntry;
    
    //获取关系信息
    if((rc = GetRelEntry(relName, relRecord, relcatEntry)))
        return (rc);

    //查找与此索引关联的属性
    RM_Record attrRecord;
    AttrCatEntry *attrcatEntry;
    //查找属性
    if((rc = FindAttr(relName, attrName, attrRecord, attrcatEntry))){
        return (rc);
    }

    //检查是否还有索引
    if(attrcatEntry->indexNo != NO_INDEXES)
        return (SM_INDEXEDALREADY);

    //创建此索引
    if((rc = ixManager.CreateIndex(relName, relcatEntry->indexCurrNum, attrcatEntry->attrType, attrcatEntry->attrLength)))
        return (rc);

    //准备扫描与该关系关联的文件
    IX_IndexHandle indexHandle;
    RM_FileHandle fileHandle;
    RM_FileScan fileScan;
    
    //打开索引
    if((rc = ixManager.OpenIndex(relName, relcatEntry->indexCurrNum, indexHandle)))
        return (rc);
    //打开文件
    if((rc = rmManager.OpenFile(relName, fileHandle)))
        return (rc);

    //扫描整个文件：
    if((rc = fileScan.OpenScan(fileHandle, INT, 4, 0, NO_OP, NULL))){
        return (rc);
    }
    
    RM_Record record;
    //如果有下一条记录
    while(fileScan.GetNextRec(record) != RM_EOF){
        char *pData;
        RID rid;
        //检索记录
        if((rc = record.GetData(pData) || (rc = record.GetRid(rid))))
            return (rc);
        //插入索引
        if((rc = indexHandle.InsertEntry(pData+ attrcatEntry->offset, rid)))
            return (rc);
    }
    //关闭打开的扫描器、文件和索引
    if((rc = fileScan.CloseScan()) || (rc = rmManager.CloseFile(fileHandle)) || (rc = ixManager.CloseIndex(indexHandle)))
        return (rc);
    
    //重写attrcat和relcat中属性和关系项
    attrcatEntry->indexNo = relcatEntry->indexCurrNum;
    relcatEntry->indexCurrNum++;
    relcatEntry->indexCount++;

    //更新记录
    if((rc = relcatFH.UpdateRec(relRecord)) || (rc = attrcatFH.UpdateRec(attrRecord)))
        return (rc);
    //强制写回磁盘
    if((rc = relcatFH.ForcePages() || (rc = attrcatFH.ForcePages())))
        return (rc);

    return (0);
}

/**
 2. 销毁索引
 输入参数：relName关系名称，attrName属性名称
*/
RC SM_Manager::DropIndex(const char *relName,
                         const char *attrName) {
    //输出提示
    cout << "DropIndex\n"
        << "   relName =" << relName << "\n"
        << "   attrName=" << attrName << "\n";
    
    RC rc = 0;
    RM_Record relRecord;
    RelCatEntry *relcatEntry;
    
    //检索与relcat中relName相关的记录和数据
    if((rc = GetRelEntry(relName, relRecord, relcatEntry)))
        return (rc);

    //找到合适的属性
    RM_Record attrRecord;
    AttrCatEntry *attrcatEntry;
    //找到属性
    if((rc = FindAttr(relName, attrName, attrRecord, attrcatEntry))){
        return (rc);
    }

    //检查是否确实有索引
    if((attrcatEntry->indexNo == NO_INDEXES))
        return (SM_NOINDEX);
  
    //销毁索引
    if((rc = ixManager.DestroyIndex(relName, attrcatEntry->indexNo)))
        return (rc);

    //更新关系和属性记录中的条目
    attrcatEntry->indexNo = NO_INDEXES;
    relcatEntry->indexCount--;

    //更新两个目录页
    if((rc = relcatFH.UpdateRec(relRecord)) || (rc = attrcatFH.UpdateRec(attrRecord)))
        return (rc);
    //强制写回磁盘
    if((rc = relcatFH.ForcePages() || (rc = attrcatFH.ForcePages())))
        return (rc);

    return (0);
}


#pragma mark - 数据库操作 目录
/**
 1.检索与relcat中特定关系相关的记录和数据
 输入参数：relName给定关系名称，relRec记录接口项，entry关系目录项
 */
RC SM_Manager::GetRelEntry(const char *relName,
                           RM_Record &relRec,
                           RelCatEntry *&entry) {
    RC rc = 0;
    //使用扫描进行搜索
    RM_FileScan fs;
    if((rc = fs.OpenScan(relcatFH, STRING, MAXNAME+1, 0, EQ_OP, const_cast<char*>(relName))))
        return (rc);
    //应该只有一项
    if((rc = fs.GetNextRec(relRec)))
        return (SM_BADRELNAME);
    //关闭扫描
    if((rc = fs.CloseScan()))
        return (rc);
    //检索其数据内容
    if((rc = relRec.GetData((char *&)entry)))
        return (rc);

    return (0);
}

/**
 2.给定一个RelCatEntry，它将使用有关其所有属性的信息填充aEntry。同时还会更新属性到关系的映射
 输入参数：relEntry给定关系目录项，aEntry待更新的属性目录项
*/
RC SM_Manager::GetAttrForRel(RelCatEntry *relEntry,
                             AttrCatEntry *aEntry,
                             std::map<std::string,
                             std::set<std::string> > &attrToRel) {
    RC rc = 0;
    //遍历此关系中的所有属性
    SM_Scan attrScan;
    //创建与relEntry->relName相关的属性的扫描
    if((rc = attrScan.OpenScan(attrcatFH, (relEntry->relName))))
        return (rc);
    
    RM_Record attrRecord;
    AttrCatEntry *attrcatEntry;
    //遍历每个属性
    for(int i = 0; i < relEntry->attrCount; i++){
        //对于每个属性，获取其AttrCatEntry
        if((rc = attrScan.GetNextAttr(attrRecord, attrcatEntry)))
            return (rc);
        //获取属性数量
        int slot = attrcatEntry->attrNum;
        //初始化
        *(aEntry + slot) = (AttrCatEntry) {"\0", "\0", 0, INT, 0, 0, 0};
        //内存赋值函数
        memcpy((char *)(aEntry + slot), (char *)attrcatEntry, sizeof(AttrCatEntry));

        //将此属性添加到从属性名称到具有该属性名称的关系集的映射
        string attrString(aEntry[slot].attrName);
        string relString(relEntry->relName);
        map<string, set<string> >::iterator it = attrToRel.find(attrString);
        
        //如果此属性尚未设置，需要创建它
        if(it == attrToRel.end()){
            set<string> relNames;
            relNames.insert(relString);
            attrToRel.insert({attrString, relNames});
        }
        //已创建，只需添加即可
        else{
            attrToRel[attrString].insert(relString);
        }
    }
    //搜索成功 关闭
    if((rc = attrScan.CloseScan()))
        return (rc);

    return (0);
}

/**
 3.给定一个关系列表，它检索与它们关联的所有relCatEntries放置它们在relEntries指定的列表中。 同时还会返回所有 关系合并，并在relEntries中填充从关系名称到索引号的映射
 输入参数：relEntries给定关系目录项指针，nRelations给定关系列表中包含关系的数量，relations给定关系列表，relToInt映射
*/
RC SM_Manager::GetAllRels(RelCatEntry *relEntries,
                          int nRelations,
                          const char * const relations[],
                          int &attrCount,
                          map<string, int> &relToInt) {
    RC rc = 0;
    //遍历给定关系列表的每一个关系
    for(int i=0; i < nRelations; i++){
        RelCatEntry *relcatEntry;
        RM_Record record;
        //检索与relcat中relations[i]相关的记录和数据
        if((rc = GetRelEntry(relations[i], record, relcatEntry)))
            return (rc);
        //初始化
        *(relEntries + i) = (RelCatEntry) {"\0", 0, 0, 0, 0};
        //拷贝赋值
        memcpy((char *)(relEntries + i), (char *)relcatEntry, sizeof(RelCatEntry));
        attrCount += relEntries[i].attrCount;

        //按顺序创建从关系名称到number的映射
        string relString(relEntries[i].relName);
        relToInt.insert({relString, i});
    }
    return (rc);
}

/**
 4.此函数返回特定记录和其数据特定关系中的属性
 输入参数：relName给定关系名称，attrName给定属性名称，attrRec记录接口项，entry关系目录项
*/
RC SM_Manager::FindAttr(const char *relName,
                        const char *attrName,
                        RM_Record &attrRec,
                        AttrCatEntry *&entry) {
    RC rc = 0;
    RM_Record relRecord;
    RelCatEntry * relcatEntry;
    //检索与relcat中relName相关的记录和数据
    if((rc = GetRelEntry(relName, relRecord, relcatEntry)))
        return (rc);
  
    //通过attrcat迭代relName中的属性
    SM_Scan attrScan;
    if((rc = attrScan.OpenScan(attrcatFH, const_cast<char*>(relName))))
        return (rc);
    //找到标识位
    bool notFound = true;
    //如果没找到就继续循环
    while(notFound){
        //是否找到末尾
        if((RM_EOF == attrScan.GetNextAttr(attrRec, entry))){
            break;
        }
        //检查属性名称是否匹配
        if(strncmp(entry->attrName, attrName, MAXNAME + 1) == 0){
            notFound = false;
            break;
        }
    }
    //搜索成功 关闭
    if((rc = attrScan.CloseScan()))
        return (rc);

    //如果找不到属性，则返回错误
    if(notFound == true)
        return (SM_INVALIDATTR);
    return (rc);
}

/**
 5. 向数据库的relcat插入新项
 输入参数：relName关系名称，attrCount属性数量，recSize记录大小
*/
RC SM_Manager::InsertRelCat(const char *relName,
                            int attrCount,
                            int recSize) {
    RC rc = 0;
    //创建关系目录项
    RelCatEntry* relcatEntry = (RelCatEntry *) malloc(sizeof(RelCatEntry));
    //内存赋值函数
    memset((void*)relcatEntry, 0, sizeof(*relcatEntry));
    //初始化
    *relcatEntry = (RelCatEntry) {"\0", 0, 0, 0, 0};
    //关系名称
    memcpy(relcatEntry->relName, relName, MAXNAME + 1);
    //元组长度-记录大小
    relcatEntry->tupleLength = recSize;
    //属性数量
    relcatEntry->attrCount = attrCount;
    //初始包含的索引数量
    relcatEntry->indexCount = 0;
    //初始索引开始的枚举值
    relcatEntry->indexCurrNum = 0;
    //初始化元组数
    relcatEntry->numTuples = 0;
    //统计信息尚未初始化
    relcatEntry->statsInitialized = false;

    //插入relcat
    RID relRID;
    rc = relcatFH.InsertRec((char *)relcatEntry, relRID);
    //释放内存
    free(relcatEntry);

    return rc;
}

/**
 6. 向数据库的attrcat插入新属性
 输入参数：relName关系名称，attr新属性，offset偏移量，attrNum属性数量
*/
RC SM_Manager::InsertAttrCat(const char *relName,
                             AttrInfo attr,
                             int offset,
                             int attrNum) {
    RC rc = 0;
  
    //创建属性的目录项
    AttrCatEntry *attrcatEntry = (AttrCatEntry *)malloc(sizeof(AttrCatEntry));
    //内存赋值函数
    memset((void*)attrcatEntry, 0, sizeof(*attrcatEntry));
    //初始化
    *attrcatEntry = (AttrCatEntry) {"\0", "\0", 0, INT, 0, 0, 0};
    //关系名称
    memcpy(attrcatEntry->relName, relName, MAXNAME + 1);
    //属性名称
    memcpy(attrcatEntry->attrName, attr.attrName, MAXNAME + 1);
    //属性偏移量
    attrcatEntry->offset = offset;
    //属性类型
    attrcatEntry->attrType = attr.attrType;
    //属性长度
    attrcatEntry->attrLength = attr.attrLength;
    //索引标号 初始化为无索引
    attrcatEntry->indexNo = NO_INDEXES;
    //属性在关系中的顺序标号
    attrcatEntry->attrNum = attrNum;
    //其他标识位
    attrcatEntry->numDistinct = 0;
    attrcatEntry->maxValue = FLT_MIN;
    attrcatEntry->minValue = FLT_MAX;

    //插入attrcat
    RID attrRID;
    rc = attrcatFH.InsertRec((char *)attrcatEntry, attrRID);
    //释放内存
    free(attrcatEntry);

    return rc;
}


#pragma mark - 辅助函数
/**
 1. 判断给定属性是否为正确的属性类型
 输入参数：给定属性
*/
bool SM_Manager::isValidAttrType(AttrInfo attribute){
    //获取类型
    AttrType type = attribute.attrType;
    //获取长度
    int length = attribute.attrLength;
    //判断类型和长度是否匹配
    if(type == INT && length == 4)
        return true;
    if(type == FLOAT && length == 4)
        return true;
    if(type == STRING && (length > 0) && length < MAXSTRINGLEN)
        return true;
    return false;
}

/**
 2.该方法将 paramName 标识的系统参数设置为*value 指定的值
 输入参数：paramName给定的参数名，value给定值
*/
RC SM_Manager::Set(const char *paramName,
                   const char *value) {
    RC rc = 0;
    //输出提示
    cout << "Set\n"
         << "   paramName=" << paramName << "\n"
         << "   value    =" << value << "\n";
    //分情况 向printIndex赋值
    if(strncmp(paramName, "printIndex", 10) == 0 && strncmp(value, "true", 4) ==0){
      printIndex = true;
      return (0);
    }
    else if(strncmp(paramName, "printIndex", 10) == 0 && strncmp(value, "false", 5) ==0){
      printIndex = false;
      return (0);
    }
    if(strncmp(paramName, "printPageStats", 14) == 0  && strncmp(value, "true", 4) == 0){
      printPageStats = true;
      return (0);
    }
    if(strncmp(paramName, "printPageStats", 14) == 0  && strncmp(value, "false", 4) == 0){
      printPageStats = false;
      return (0);
    }
    //如果当前参数是打印页面统计信息
    if(strncmp(paramName, "printPageStats", 14) == 0 ){
      int *piGP = pStatisticsMgr->Get(PF_GETPAGE);
      int *piPF = pStatisticsMgr->Get(PF_PAGEFOUND);
      int *piPNF = pStatisticsMgr->Get(PF_PAGENOTFOUND);

      cout << "PF Layer Statistics" << endl;
      cout << "-------------------" << endl;
      if(piGP)
        cout << "Total number of calls to GetPage Routine: " << *piGP << endl;
      else
        cout << "Total number of calls to GetPage Routine: None" << endl;
      if(piPF)
        cout << "  Number found: " << *piPF << endl;
      else
        cout << "  Number found: None" << endl;
      if(piPNF)
        cout << "  Number not found: " << *piPNF << endl;
      else
        cout << "  Number found: None" << endl;
      return (0);
    }
    if(strncmp(paramName, "resetPageStats", 14) == 0){
      pStatisticsMgr->Reset();
      return (0);
    }
    
    //分情况 向useQO赋值
    if(strncmp(paramName, "useQO", 5) == 0 && strncmp(value, "true", 4) ==0){
      cout << "Using QO" << endl;
      useQO = true;
      return (0);
    }
    if(strncmp(paramName, "useQO", 5) == 0 && strncmp(value, "false", 5) ==0){
      cout << "disabling QO" << endl;
      useQO = false;
      return (0);
    }
    //如果当前参数是打印统计信息
    if(strncmp(paramName, "printStats", 10) == 0){
      PrintStats(value);
      return (0);
    }
    //如果当前参数是计算统计信息
    if(strncmp(paramName, "calcStats", 9) == 0){
      CalcStats(value);
      return (0);
    }


    return (SM_BADSET);
}

/**
 3.重置m页面统计信息
 输入参数：无
*/
RC SM_Manager::ResetPageStats(){
    pStatisticsMgr->Reset();
    return (0);
}

/**
 4.将string转换成float
 输入参数：给定的string
*/
float SM_Manager::ConvertStrToFloat(char *string){
    float value = (float) string[0];
    return value;
}

/**
 5.计算统计信息
 输入参数：relName给定的关系名
*/
RC SM_Manager::CalcStats(const char *relName){
    RC rc = 0;
    //输出提示
    cout << "Calculating stats for relation " << relName << endl;
    //检查是否为有效关系名
    if(strlen(relName) > MAXNAME)
        return (SM_BADRELNAME);

    //检索与关系关联的记录
    RM_Record relRecord;
    RelCatEntry *relcatEntry;
    //检索与relcat中relName相关的记录和数据
    if((rc = GetRelEntry(relName, relRecord, relcatEntry)))
        return (rc);

    //创建一个包含有关属性信息的结构,帮助加载
    Attr* attributes = (Attr *)malloc(sizeof(Attr)*relcatEntry->attrCount);
    for(int i=0; i < relcatEntry->attrCount; i++){
        memset((void*)&attributes[i], 0, sizeof(attributes[i]));
        IX_IndexHandle indexHandle;
        attributes[i] = (Attr) {0, 0, 0, 0, indexHandle, recInsert_string, 0, FLT_MIN, FLT_MAX};
    }
    //设置“属性”列表
    if((rc = PrepareAttr(relcatEntry, attributes)))
        return (rc);

    vector<set<string> > numDistinct(relcatEntry->attrCount);
    //遍历属性
    for(int i=0; i < relcatEntry->attrCount; i++){
        attributes[i].numDistinct = 0;
        attributes[i].maxValue = FLT_MIN;
        attributes[i].minValue = FLT_MAX;
    }
    relcatEntry->numTuples = 0;
    relcatEntry->statsInitialized = true;

    //打开关系并对其进行迭代
    RM_FileScan fileScan;
    RM_FileHandle fileHandle;
    RM_Record record;
    //打开文件并打开扫描
    if((rc = rmManager.OpenFile(relName, fileHandle)) || (rc = fileScan.OpenScan(fileHandle, INT, 0, 0, NO_OP, NULL)))
        return (rc);
    //获取下一条记录
    while(RM_EOF != fileScan.GetNextRec(record)){
        char * recData;
        //获取数据
        if((rc = record.GetData(recData)))
            return (rc);

        //遍历属性
        for(int i = 0;  i < relcatEntry->attrCount; i++){
            int offset = attributes[i].offset;
            string attr(recData + offset, recData + offset + attributes[i].length);
            numDistinct[i].insert(attr);
            float attrValue = 0.0;
            if(attributes[i].type == STRING)
                attrValue = ConvertStrToFloat(recData + offset);
            else if(attributes[i].type == INT)
                attrValue = (float) *((int*) (recData + offset));
            else
                attrValue = *((float*)(recData + offset));
            if(attrValue > attributes[i].maxValue)
                attributes[i].maxValue = attrValue;
            if(attrValue < attributes[i].minValue)
                attributes[i].minValue = attrValue;
        }
        relcatEntry->numTuples++;
    }

    //将所有内容写回磁盘
    if((rc = relcatFH.UpdateRec(relRecord)) || (rc = relcatFH.ForcePages()))
        return (rc);

    SM_Scan attrScan;
    //打开扫描
    if((rc = attrScan.OpenScan(attrcatFH, relcatEntry->relName)))
        return (rc);
    RM_Record attrRecord;
    AttrCatEntry *attrcatEntry;
    //遍历属性
    for(int i = 0; i < relcatEntry->attrCount; i++){
        if((rc = attrScan.GetNextAttr(attrRecord, attrcatEntry)))
            return (rc);
        //对于每个属性，请将其信息放在适当的位置
        int slot = attrcatEntry->attrNum;
        attrcatEntry->minValue = attributes[slot].minValue;
        attrcatEntry->maxValue = attributes[slot].maxValue;
        attrcatEntry->numDistinct = numDistinct[slot].size();
        if((rc = attrcatFH.UpdateRec(attrRecord)))
            return (rc);
    }
    //关闭扫描
    if((rc = attrScan.CloseScan()))
        return (rc);
    //强制写回磁盘
    if((rc = attrcatFH.ForcePages()))
        return (rc);

    return (0);
}


#pragma mark - 数据库操作 加载
/**
 1.设置“属性”列表（用于保存信息的结构，关于有助于加载文件的属性）
 输入参数：rEntry关系目录项，attributes属性信息
 */
RC SM_Manager::PrepareAttr(RelCatEntry *rEntry,
                           Attr* attributes) {
    RC rc = 0;
    //遍历与此关系相关的属性
    SM_Scan attrScan;
    //创建与rEntry->relName相关的属性的扫描
    if((rc = attrScan.OpenScan(attrcatFH, rEntry->relName)))
        return (rc);
    RM_Record attrRecord;
    AttrCatEntry *attrcatEntry;
    //遍历属性
    for(int i = 0; i < rEntry->attrCount; i++){
        //获取与之关联的下一个属性
        if((rc = attrScan.GetNextAttr(attrRecord, attrcatEntry)))
            return (rc);
        //对于每个属性，请将其信息放在适当的位置
        int slot = attrcatEntry->attrNum;
        //更新属性信息
        attributes[slot].offset = attrcatEntry->offset;
        attributes[slot].type = attrcatEntry->attrType;
        attributes[slot].length = attrcatEntry->attrLength;
        attributes[slot].indexNo = attrcatEntry->indexNo;
        attributes[slot].numDistinct = attrcatEntry->numDistinct;
        attributes[slot].maxValue = attrcatEntry->maxValue;
        attributes[slot].minValue = attrcatEntry->minValue;

        //如果有关联则打开索引
        if((attrcatEntry->indexNo != NO_INDEXES)){
            IX_IndexHandle indexHandle;
            attributes[slot].ih = indexHandle;
            if((rc = ixManager.OpenIndex(rEntry->relName, attrcatEntry->indexNo, attributes[slot].ih)))
                return (rc);
        }

        //确保解析器指向（取决于属性类型）
        if(attrcatEntry->attrType == INT){
            attributes[slot].recInsert = &recInsert_int;
        }
        else if(attrcatEntry->attrType == FLOAT)
            attributes[slot].recInsert = &recInsert_float;
        else
            attributes[slot].recInsert = &recInsert_string;
    }
    //关闭
    if((rc = attrScan.CloseScan()))
        return (rc);
    return (0);
}

/**
 2.将内容从指定文件加载到指定关系中
 输入参数：relName给定关系名，fileName给定文件名
*/
RC SM_Manager::Load(const char *relName,
                    const char *fileName) {
    //输出提示
    cout << "Load\n"
         << "   relName =" << relName << "\n"
         << "   fileName=" << fileName << "\n";

    RC rc = 0;
    RM_Record relRecord;
    RelCatEntry *relcatEntry;
    //检索与relcat中relName相关的记录和数据
    if((rc = GetRelEntry(relName, relRecord, relcatEntry)))
        return (rc);
    if(relcatEntry->statsInitialized == false)
        calcStats = true;

    //创建一个包含有关属性信息的结构,帮助加载
    Attr* attributes = (Attr *)malloc(sizeof(Attr)*relcatEntry->attrCount);
    //遍历relcatEntry中的属性
    for(int i = 0; i < relcatEntry->attrCount; i++) {
        //拷贝赋值
        memset((void*)&attributes[i], 0, sizeof(attributes[i]));
        IX_IndexHandle indexHandle;
        attributes[i] = (Attr) {0, 0, 0, 0, indexHandle, recInsert_string, 0, FLT_MAX, FLT_MIN};
    }
    
    //设置“属性”列表
    if((rc = PrepareAttr(relcatEntry, attributes)))
        return (rc);

    //打开文件并加载内容
    RM_FileHandle relFileHandle;
    if((rc = rmManager.OpenFile(relName, relFileHandle)))
        return (rc);
    int totalRecordNum = 0;
    rc = OpenAndLoadFile(relFileHandle, fileName,
                         attributes,
                         relcatEntry->attrCount,
                         relcatEntry->tupleLength,
                         totalRecordNum);
    RC rc2;

    //写回属性和rel统计信息
    if(calcStats){
        //元组数
        relcatEntry->numTuples = totalRecordNum;
        //统计初始化设置标识位
        relcatEntry->statsInitialized = true;
        //更新并强制写回磁盘
        if((rc = relcatFH.UpdateRec(relRecord)) || (rc = relcatFH.ForcePages()))
            return (rc);

        SM_Scan attrScan;
        //打开扫描
        if((rc = attrScan.OpenScan(attrcatFH, relcatEntry->relName)))
            return (rc);
        
        RM_Record attrRecord;
        AttrCatEntry *attrcatEntry;
        //遍历属性
        for(int i = 0; i < relcatEntry->attrCount; i++){
            //获取与之关联的下一个属性
            if((rc = attrScan.GetNextAttr(attrRecord, attrcatEntry)))
                return (rc);
            //对于每个属性，请将其信息放在适当的位置
            int slot = attrcatEntry->attrNum;
            attrcatEntry->minValue = attributes[slot].minValue;
            attrcatEntry->maxValue = attributes[slot].maxValue;
            attrcatEntry->numDistinct = attributes[slot].numDistinct;
            
            if((rc = attrcatFH.UpdateRec(attrRecord)))
                return (rc);
        }
        //关闭扫描
        if((rc = attrScan.CloseScan()))
            return (rc);
        //强制写回磁盘
        if((rc = attrcatFH.ForcePages()))
            return (rc);
        calcStats = false;
    }

    //销毁并关闭Attribute结构中的指针
    if((rc2 = CleanUpAttr(attributes, relcatEntry->attrCount)))
        return (rc2);

    //关闭文件
    if((rc2 = rmManager.CloseFile(relFileHandle)))
        return (rc2);

    return (rc);
}

/**
 3.打开并加载文件到表中loadedRecs
 输入参数：relFH给定的文件处理器，fileName给定文件名，attributes属性列表，attrCount属性数量，recLength记录长度，loadedRecs被加载的记录
*/
RC SM_Manager::OpenAndLoadFile(RM_FileHandle &relFH,
                               const char *fileName,
                               Attr* attributes,
                               int attrCount,
                               int recLength,
                               int &loadedRecs) {
    RC rc = 0;
    loadedRecs = 0;

    char *record = (char *)calloc(recLength, 1);

    //打开加载文件
    ifstream f(fileName);
    //如果打开失败
    if(f.fail()){
        cout << "cannot open file :( " << endl;
        free(record);
        return (SM_BADLOADFILE);
    }

    vector<set<string> > numDistinct(attrCount);
 
    //用逗号分隔的元组
    string line, token;
    string delimiter = ",";
    
    //加载文件，一次读入一行
    while (getline(f, line)) {
        RID recRID;
        //遍历每个属性
        for(int i = 0; i < attrCount; i++){
            if(line.size() == 0){
                free(record);
                f.close();
                return (SM_BADLOADFILE);
            }
        
            //找到下一个定界符的值并截断它
            size_t pos = line.find(delimiter);
            if(pos == string::npos)
                pos = line.size();
            token = line.substr(0, pos);
            line.erase(0, pos + delimiter.length());

            //解析属性值，然后将其插入右侧的插槽中
            //如果解析不正确，则recInsert应该返回false
            if(attributes[i].recInsert(record + attributes[i].offset, token, attributes[i].length) == false){
                rc = SM_BADLOADFILE;
                free(record);
                f.close();
                return (rc);
            }
        }
        //将记录插入文件
        if((rc = relFH.InsertRec(record, recRID))){
            free(record);
            f.close();
            return (rc);
        }

        //将记录的各个部分插入适当的索引
        for(int i=0; i < attrCount; i++){
            if(attributes[i].indexNo != NO_INDEXES){
                if((rc = attributes[i].ih.InsertEntry(record + attributes[i].offset, recRID))){
                    free(record);
                    f.close();
                    return (rc);
                }
            }
            //如果统计信息开启
            if(calcStats){
                int offset = attributes[i].offset;
                string attr(record + offset, record + offset + attributes[i].length);
                numDistinct[i].insert(attr);
                float attrValue = 0.0;
                if(attributes[i].type == STRING)
                    attrValue = ConvertStrToFloat(record + offset);
                else if(attributes[i].type == INT)
                    attrValue = (float) *((int*) (record + offset));
                else{
                    attrValue = *((float*) (record + offset));
                }
                if(attrValue > attributes[i].maxValue)
                    attributes[i].maxValue = attrValue;
                if(attrValue < attributes[i].minValue)
                    attributes[i].minValue = attrValue;
            }
        }
        //加载的记录数加一
        loadedRecs++;
        //printf("record : %d, %d\n", *(int*)record, *(int*)(record+4));
    }
    
    for(int i=0; i < attrCount; i++){
        attributes[i].numDistinct = numDistinct[i].size();
        //printf("num attributes: %d for index %d \n", attributes[i].numDistinct, i);
    }

cleanup:
    //释放内存并关闭文件
    free(record);
    f.close();

    return (rc);
}

/**
 4.清理用于加载值的属性的结构
 输入参数：attributes“属性”列表，attrCount属性数量
*/
RC SM_Manager::CleanUpAttr(Attr* attributes,
                           int attrCount) {
    RC rc = 0;
    //遍历清理属性
    for(int i=0; i < attrCount; i++){
        if(attributes[i].indexNo != NO_INDEXES){
            //关闭索引
            if((rc = ixManager.CloseIndex(attributes[i].ih)))
                return (rc);
        }
    }
    //释放内存
    free(attributes);
    return (rc);
}


#pragma mark - 数据库操作 打印
/**
 1.打印关系中的所有元组
 输入参数：relName关系名称
*/
RC SM_Manager::Print(const char *relName) {
    //打印提示
    cout << "Print\n"
        << "   relName=" << relName << "\n";

    RC rc = 0;
    RM_Record relRecord;
    RelCatEntry *relcatEntry;
    
    //检索与relcat中relName相关的记录和数据
    if((rc = GetRelEntry(relName, relRecord, relcatEntry)))
        return (SM_BADRELNAME);
    //设置属性数
    int numAttr = relcatEntry->attrCount;

    //设置要打印的DataAttrInfo
    DataAttrInfo * attributes = (DataAttrInfo *)malloc(numAttr* sizeof(DataAttrInfo));
    //遍历关系中的属性，并进行设置用于打印的DataAttrInfo
    if((rc = SetUpPrint(relcatEntry, attributes)))
        return (rc);
    
    //构造Printer
    Printer printer(attributes, relcatEntry->attrCount);
    //打印header
    printer.PrintHeader(cout);

    //打开文件，然后扫描整个文件
    RM_FileHandle fileHandle;
    RM_FileScan fileScan;
    //打开文件并打开扫描
    if((rc = rmManager.OpenFile(relName, fileHandle)) || (rc = fileScan.OpenScan(fileHandle, INT, 4, 0, NO_OP, NULL))){
        free(attributes);
        return (rc);
    }

    //检索每条记录并打印
    RM_Record record;
    //获取有关的下一条记录
    while(fileScan.GetNextRec(record) != RM_EOF){
        char *pData;
        //获取数据
        if((record.GetData(pData))){
            free(attributes);
            return (rc);
        }
        //打印
        printer.Print(cout, pData);
    }
    //关闭扫描
    fileScan.CloseScan();
    //打印footer
    printer.PrintFooter(cout);
    //释放DataAttrInfo
    free(attributes);

    return (0);
}

/**
 2.遍历关系中的属性，并进行设置用于打印的DataAttrInfo
 输入参数：rEntry给定的关系目录项，attributes属性列表
*/
RC SM_Manager::SetUpPrint(RelCatEntry* rEntry,
                          DataAttrInfo *attributes) {
    RC rc = 0;
    RID attrRID;
    RM_Record attrRecord;
    AttrCatEntry *attrcatEntry;

    //遍历attrcat以获取与此关系相关的属性
    SM_Scan attrScan;
    if((rc = attrScan.OpenScan(attrcatFH, rEntry->relName)))
        return (rc);

    //遍历属性
    for(int i = 0; i < rEntry->attrCount; i++){
        //获取有关的下一个属性
        if((rc = attrScan.GetNextAttr(attrRecord, attrcatEntry))){
            return (rc);
        }
        //在适当的位置插入其信息
        int slot = attrcatEntry->attrNum;
        //拷贝赋值
        memcpy(attributes[slot].relName, attrcatEntry->relName, MAXNAME + 1);
        memcpy(attributes[slot].attrName, attrcatEntry->attrName, MAXNAME + 1);
        //设置属性信息
        attributes[slot].offset = attrcatEntry->offset;
        attributes[slot].attrType = attrcatEntry->attrType;
        attributes[slot].attrLength = attrcatEntry->attrLength;
        attributes[slot].indexNo = attrcatEntry->indexNo;
    }
    //关闭扫描
    if((rc = attrScan.CloseScan()))
        return (rc);

    return (rc);
}

/**
 3.打印页面统计信息
 输入参数：无
*/
RC SM_Manager::PrintPageStats(){
    int *piGP = pStatisticsMgr->Get(PF_GETPAGE);
    int *piPF = pStatisticsMgr->Get(PF_PAGEFOUND);
    int *piPNF = pStatisticsMgr->Get(PF_PAGENOTFOUND);

    cout << "PF Layer Statistics" << endl;
    cout << "-------------------" << endl;
    if(piGP)
        cout << "Total number of calls to GetPage Routine: " << *piGP << endl;
    else
        cout << "Total number of calls to GetPage Routine: None" << endl;
    if(piPF)
        cout << "  Number found: " << *piPF << endl;
    else
        cout << "  Number found: None" << endl;
    if(piPNF)
        cout << "  Number not found: " << *piPNF << endl;
    else
        cout << "  Number found: None" << endl;
    return (0);
}

/**
 4.设置dataAttrInfo结构以从relcat打印
 输入参数：attributes属性列表
*/
RC SM_Manager::SetUpRelCatAttributes(DataAttrInfo *attributes) {
    int numAttr = 4;
    //遍历
    for(int i= 0; i < numAttr; i++){
        memcpy(attributes[i].relName, "relcat", strlen("relcat") + 1);
        attributes[i].indexNo = 0;
    }
  
    //拷贝赋值
    memcpy(attributes[0].attrName, "relName", MAXNAME + 1);
    memcpy(attributes[1].attrName, "tupleLength", MAXNAME + 1);
    memcpy(attributes[2].attrName, "attrCount", MAXNAME + 1);
    memcpy(attributes[3].attrName, "indexCount", MAXNAME + 1);

    //设置属性列表
    attributes[0].offset = (int) offsetof(RelCatEntry,relName);
    attributes[1].offset = (int) offsetof(RelCatEntry,tupleLength);
    attributes[2].offset = (int) offsetof(RelCatEntry,attrCount);
    attributes[3].offset = (int) offsetof(RelCatEntry,indexCount);

    attributes[0].attrType = STRING;
    attributes[1].attrType = INT;
    attributes[2].attrType = INT;
    attributes[3].attrType = INT;

    attributes[0].attrLength = MAXNAME + 1;
    attributes[1].attrLength = 4;
    attributes[2].attrLength = 4;
    attributes[3].attrLength = 4;

    return (0);
}

/**
 5.设置dataAttrInfo结构以从attrcat打印
 输入参数：attributes属性列表
*/
RC SM_Manager::SetUpAttrCatAttributes(DataAttrInfo *attributes){
    int numAttr = 6;
    //遍历
    for(int i= 0; i < numAttr; i++){
        memcpy(attributes[i].relName, "attrcat", strlen("attrcat") + 1);
        attributes[i].indexNo = 0;
    }
  
    //拷贝赋值
    memcpy(attributes[0].attrName, "relName", MAXNAME + 1);
    memcpy(attributes[1].attrName, "attrName", MAXNAME + 1);
    memcpy(attributes[2].attrName, "offset", MAXNAME + 1);
    memcpy(attributes[3].attrName, "attrType", MAXNAME + 1);
    memcpy(attributes[4].attrName, "attrLength", MAXNAME + 1);
    memcpy(attributes[5].attrName, "indexNo", MAXNAME + 1);

    //设置属性列表
    attributes[0].offset = (int) offsetof(AttrCatEntry,relName);
    attributes[1].offset = (int) offsetof(AttrCatEntry,attrName);
    attributes[2].offset = (int) offsetof(AttrCatEntry,offset);
    attributes[3].offset = (int) offsetof(AttrCatEntry,attrType);
    attributes[4].offset = (int) offsetof(AttrCatEntry,attrLength);
    attributes[5].offset = (int) offsetof(AttrCatEntry,indexNo);

    attributes[0].attrType = STRING;
    attributes[1].attrType = STRING;
    attributes[2].attrType = INT;
    attributes[3].attrType = INT;
    attributes[4].attrType = INT;
    attributes[5].attrType = INT;

    attributes[0].attrLength = MAXNAME + 1;
    attributes[1].attrLength = MAXNAME + 1;
    attributes[2].attrLength = 4;
    attributes[3].attrLength = 4;
    attributes[4].attrLength = 4;
    attributes[5].attrLength = 4;

    return (0);
}

/**
 6.打印统计信息
 输入参数：relName给定关系名
*/
RC SM_Manager::PrintStats(const char *relName){
    RC rc = 0;
    //输出提示
    cout << "Printing stats for relation " << relName << endl;
    //检查是否为有效关系名
    if(strlen(relName) > MAXNAME)
        return (SM_BADRELNAME);

    //检索与关系关联的记录
    RM_Record relRecord;
    RelCatEntry *relcatEntry;
    if((rc = GetRelEntry(relName, relRecord, relcatEntry)))
        return (rc);

    //输出提示
    cout << "Total Tuples in Relation: " << relcatEntry->numTuples << endl;
    cout << endl;
  
    AttrCatEntry *attrcatEntry;
    RM_Record attrRecord;
    SM_Scan attrScan;
    //打开扫描
    if((rc = attrScan.OpenScan(attrcatFH, relcatEntry->relName)))
        return (rc);

    //遍历属性
    for(int i=0; i < relcatEntry->attrCount; i++){
        if((rc = attrScan.GetNextAttr(attrRecord, attrcatEntry))){
            return (rc);
        }
        // int slot = aEntry-> attrNum;
        //输出提示
        cout << "  Attribute: " << attrcatEntry->attrName << endl;
        cout << "    Num attributes: " << attrcatEntry->numDistinct << endl;
        cout << "    Max value: " << attrcatEntry->maxValue << endl;
        cout << "    Min value: " << attrcatEntry->minValue << endl;
    }
    //关闭扫描
    if((rc = attrScan.CloseScan()))
        return (rc);

    return (0);
}


#pragma mark - 数据库操作 Help
/**
 1.打印有关此数据库中所有关系的信息,打印的信息包括：relName,tupleLength,attrCount,indexCount
 输入参数：无
 */
RC SM_Manager::Help() {
    //输出提示
    cout << "Help\n";
    RC rc = 0;
    // 为DataAttrInfo申请内存，进行打印并进行设置
    DataAttrInfo * attributes = (DataAttrInfo *)malloc(4* sizeof(DataAttrInfo));
    //设置打印结构
    if((rc = SetUpRelCatAttributes(attributes)))
        return (rc);
    //打印
    Printer printer(attributes, 4);
    printer.PrintHeader(cout);
    
    //打开文件扫描
    RM_FileScan fileScan;
    if((rc = fileScan.OpenScan(relcatFH, INT, 4, 0, NO_OP, NULL))){
        free(attributes);
        return (rc);
    }

    //遍历所有关系记录 并打印
    RM_Record record;
    while(fileScan.GetNextRec(record) != RM_EOF){
        char *pData;
        if((record.GetData(pData))){
            free(attributes);
            return (rc);
        }
        printer.Print(cout, pData);
    }

    //关闭并释放内存
    fileScan.CloseScan();
    printer.PrintFooter(cout);
    free(attributes);

    return (0);
}

/**
 2.打印有关指定关系中所有属性的信息, 打印的信息包括：relName,attributeName,偏移,属性类型,属性长度,索引号
 输入参数：relName给定关系名
*/
RC SM_Manager::Help(const char *relName) {
    //输出提示
    cout << "Help\n"
         << "   relName =" << relName << "\n";
    RC rc = 0;
    RM_FileScan fs;
    RM_Record rec;

    //检查此关系是否存在
    if((rc = fs.OpenScan(relcatFH, STRING, MAXNAME+1, 0, EQ_OP, const_cast<char*>(relName))))
        return (rc);
    if(fs.GetNextRec(rec) == RM_EOF){
        fs.CloseScan();
        return (SM_BADRELNAME);
    }
    fs.CloseScan();
  

    //设置要打印的DataAttrInfo
    DataAttrInfo * attributes = (DataAttrInfo *)malloc(6* sizeof(DataAttrInfo));
    if((rc = SetUpAttrCatAttributes(attributes)))
        return (rc);
    //打印
    Printer printer(attributes, 6);
    printer.PrintHeader(cout);

    //遍历attrcat以查找与此关系相关联所有属性 并打印
    if((rc = fs.OpenScan(attrcatFH, STRING, MAXNAME+1, 0, EQ_OP, const_cast<char*>(relName))))
        return (rc);

    //如果有下一条记录
    while(fs.GetNextRec(rec) != RM_EOF){
        char *pData;
        if((rec.GetData(pData)))
            return (rc);
        printer.Print(cout, pData);
    }
    
    if((rc = fs.CloseScan() ))
        return (rc);

    //如果要打印索引，请再次遍历，并打印整个索引
    if((rc = fs.OpenScan(attrcatFH, STRING, MAXNAME+1, 0, EQ_OP, const_cast<char*>(relName))))
        return (rc);
    while(fs.GetNextRec(rec) != RM_EOF){
        char *pData;
        if((rec.GetData(pData)))
            return (rc);
        if(printIndex){
            AttrCatEntry *attr = (AttrCatEntry*)pData;
            if((attr->indexNo != NO_INDEXES)){
                IX_IndexHandle ih;
                if((rc = ixManager.OpenIndex(relName, attr->indexNo, ih)))
                    return (rc);
        
                //printf("successfully opens \n");
                //打印后关闭索引
                if((rc = ih.PrintIndex()) || (rc = ixManager.CloseIndex(ih)))
                    return (rc);
            }
        }
    }
    
    //打印footer
    printer.PrintFooter(cout);
    //释放内存
    free(attributes);
    return (0);
}
