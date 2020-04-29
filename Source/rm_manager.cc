//  rm_manager.cc
//  DataBaseSystem
//  记录管理模块的主接口
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//
#include <unistd.h>
#include <sys/types.h>
#include <cstring>
#include "../Header/pf.h"
#include "../Header/rm_manager.h"

/**
 * 关联文件系统模块
 * @param pfm
 */
RM_Manager::RM_Manager(PF_Manager &pfm) : pfm(pfm){
}

RM_Manager::~RM_Manager()=default;

/**
 * 创建一个记录长度为recordSize的数据表
 * 将数据表的信息写入PF_FileHandle那页页首部之后的剩余空间
 * 1.检查，传入参数是否合法
 * 2.调用文件系统模块，在底层创建一个数据表的文件，占一个页的空间
 * 3.讲数据表的头信息写入这个文件的中
 * @param fileName      数据表名
 * @param recordSize    记录长度
 * @return
 */
RC RM_Manager::CreateFile (const char *fileName, int recordSize) {
    RC rc = OK_RC;
    // 数据表名不能为空
    if(fileName == NULL)
        return RM_BADFILENAME;
    // 记录长度限制,一条记录不能超过1页
    if(recordSize <= 0 || recordSize > PF_PAGE_SIZE)
        return RM_BADRECORDSIZE;

    // 这里具体数值可能会出现问题
    // 每页的记录上限
    int numRecordsPerPage = (int)((PF_PAGE_SIZE - sizeof(RM_PageHeader)) / (recordSize + 1.0 / 8));
    // bitMap占用的字节数
    int bitmapSize = (numRecordsPerPage + 1) / 8;

    if( (PF_PAGE_SIZE - bitmapSize - sizeof(RM_PageHeader))/recordSize <= 0)
        return RM_BADRECORDSIZE;


    // 调用文件系统模块创建文件,该操作会在底层磁盘上创建一个文件
    if((rc = pfm.CreateFile(fileName)))
        return (rc);

    // 不需要通过缓冲区，直接向磁盘的对应地区写入数据表的必要信息
    // 打开文件，得到文件描述符，以便向磁盘写入数据
    // 将file强制转换成PF_FileHandle类型
    PF_FileHandle pfFileHandle;

    RM_FileHeader *header;
    // 打开这个文件，写入数据表头信息
    // OpenFile会从磁盘上将文件头页加载到缓冲区
    // 即得到了这个文件的信息
    if((rc = pfm.OpenFile(fileName, pfFileHandle)))  //pin=1
        return (rc);

    // 得到这个文件的表头页，也就是页号为-1的页，此操作会在缓冲池中申请一个页的空间，用来存放表头页
    // 表头会被封装成页
    PF_PageHandle fileHdrPage;
    if((rc = pfFileHandle.GetThisPage(PF_FILE_HDR_PAGENUM,fileHdrPage))) //pin=2
        return rc;

    // 得到数据的地址
    char * hdrPage;
    fileHdrPage.GetPageData(hdrPage);

    // 从文件头之后的内容就是数据表头
    header = (RM_FileHeader *) hdrPage;
    header->recordSize = recordSize;
    header->numRecordsPerPage = numRecordsPerPage;
    header->bitmapLen = bitmapSize;
    header->firstFreePage = NO_MORE_FREE_PAGES;
    header->numPages = 0;

    // 缓冲区中的表头被修改，创建表的工作不是经常的，以防万一
    pfFileHandle.SetHdrChanged();

    // 在函数结束时，关闭文件，将表头页回写
    RC rc2 = OK_RC;
    if((rc2 = pfFileHandle.MarkDirty(PF_FILE_HDR_PAGENUM)) || (rc2 = pfFileHandle.UnpinPage(PF_FILE_HDR_PAGENUM))
    || (rc2 = pfm.CloseFile(pfFileHandle)))
        return rc2;

    return OK_RC;
}

// 删除文件，调用文件管理模块删除文件
RC RM_Manager::DestroyFile(const char *fileName) {
    if(fileName == NULL)
        return RM_BADFILENAME;
    RC rc;
    if((rc = pfm.DestroyFile(fileName)))
        return rc;
  return OK_RC;
}

/**
 * 当打开数据表的时候，要将数据表的头信息RM_FileHeader载入,
 * 并将数据表的pfh指向PF_FileHandle
 * @param fileHandle 数据表对象
 * @param fh         文件对象
 * @param header     数据表头
 * @return
 */
RC RM_Manager::SetUpFH(RM_FileHandle& fileHandle, PF_FileHandle &fh, RM_FileHeader* header){
    // 数据表的一些信息写入到数据表头
    memcpy(&fileHandle.tableHeader, header, sizeof(RM_FileHeader));
    // 数据表与文件关联起来
    fileHandle.pfh = &fh;
    // 因为修改了数据表头的信息，头信息修改位设置为TRUE
    fileHandle.ifHeaderModified = TRUE;
    fileHandle.isOpened = TRUE;

    //  确认数据表头信息可用
    if(! fileHandle.isValidFileHeader()){
        fileHandle.isOpened = FALSE;
            return RM_INVALIDFILE;
    }
    return OK_RC;
}


/**
 * 打开一个数据表，在内存中创建一个数据表对象
 * 再将头文件加载到缓冲区中
 * 1.检查 文件名非法，数据表是否打开
 * 2.根据文件名打开文件，并调用getthispage方法，讲页加载到缓冲区中
 * 3.提出页中的数据表的信息，将RM_FileHandle与PF_FileHandle关联起来
 * @param fileName
 * @param fileHandle
 * @return
 */
RC RM_Manager::OpenFile   (const char *fileName, RM_FileHandle &fileHandle){
    if(fileName == NULL)
        return RM_BADFILENAME;
    if(fileHandle.isOpened == true)
        return RM_INVALIDFILEHANDLE;

    RC rc;
    // 打开文件, file必须被保存在内存中，当close数据表时清除
    PF_FileHandle file;  // 在栈中创建一个文件对象，之后需要将其保存在内存上
    // 打开文件，将文件头页加载进内存，pfFileHandle构造出文件对象
    if((rc = pfm.OpenFile(fileName, file)))
        return rc;

    // 在缓冲区中申请一块地址来保存file;
    char *pf;
    pfm.AllocateBlock(pf);        //pf为内存中的地址

    // 复制
    memcpy(pf,&file,sizeof(PF_FileHandle));
    PF_FileHandle *pfFileHandle = (PF_FileHandle *)pf;

    PF_PageHandle fileHdrPage;
    PageNum pageNum;
    if((rc = pfFileHandle->GetThisPage(PF_FILE_HDR_PAGENUM,fileHdrPage))){
        // 如果失败了，unping这个页，关闭这个文件，并且释放空间
        pfFileHandle->UnpinPage(fileHdrPage.GetPageNum());
        pfm.CloseFile(*pfFileHandle);
        pfm.DisposeBlock((char *)pfFileHandle);
        return rc;
    }

    // 得到文件头页中数据表头的地址，
    char * hdrPage;
    fileHdrPage.GetPageData(hdrPage);
    RM_FileHeader* hdr = (RM_FileHeader *)hdrPage;

    // 关联
    SetUpFH(fileHandle, *pfFileHandle, hdr);

    RC rc2;

    if((rc2 = pfFileHandle->UnpinPage(fileHdrPage.GetPageNum())))
        return (rc2);

    if(rc != OK_RC){
        pfm.CloseFile(*pfFileHandle);
    }
    // 设置打开标志
    fileHandle.isOpened = TRUE;
    return OK_RC;
}


RC RM_Manager::CleanUpFH(RM_FileHandle &fileHandle){
    // 已经关闭的不能再次关闭
    if(fileHandle.isOpened == FALSE)
        return RM_INVALIDFILEHANDLE;
    fileHandle.isOpened = FALSE;
    fileHandle.ifHeaderModified = FALSE;
    return OK_RC;
}


/**
 * 关闭fileHandle的数据表,若表头信息有修改，则回写进磁盘
 * 1.检查fileHandle是否已经关闭，关闭的页不能再次关闭
 * @param fileHandle
 * @return
 */
RC RM_Manager::CloseFile  (RM_FileHandle &fileHandle) {
    RC rc;
    // 检查
    if(fileHandle.isOpened == FALSE)
        return RM_INVALIDFILEHANDLE;

    // 如果数据表头信息被修改
    PF_PageHandle fileHdrPage;
    if(fileHandle.ifHeaderModified == TRUE){
        //将RM_FileHandle.RM_FileHdr中修改的数据拷贝到对应的文件头页中
        if((rc = fileHandle.pfh->GetThisPage(PF_FILE_HDR_PAGENUM,fileHdrPage)))
            return rc;

        // 找到文件头数据之后的地址
        char *pPage;
        fileHdrPage.GetPageData(pPage);

        // 复制RM_FileHandle.RM_Hdr中的内容到PF_Hdr之后
        memcpy(pPage, &fileHandle.tableHeader, sizeof(RM_FileHeader));
        // 将缓冲区中的文件头页标记为脏页，并unpin
        if((rc = fileHandle.pfh->MarkDirty(PF_FILE_HDR_PAGENUM))
        || (rc = fileHandle.pfh->UnpinPage(PF_FILE_HDR_PAGENUM)))
            return (rc);
    }
    // 如果没有被修改就啥都不用做

    // 关闭文件
    if((rc = pfm.CloseFile(*fileHandle.pfh)))
        return rc;

    // 由于OpenFile时在缓冲区中申请了空间，需要释放掉
    if(fileHandle.pfh != NULL)
        pfm.DisposeBlock((char *)fileHandle.pfh);
    // 数据表清除
    if((rc = CleanUpFH(fileHandle)))
        return (rc);
    return OK_RC;
}
