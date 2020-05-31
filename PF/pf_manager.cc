// File:        pf_manager.cc
// Description: PF_Manager class implementation


#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "pf_internal.h"
#include "pf_buffermgr.h"

//
// PF_Manager
//
// Desc: Constructor - intended to be called once at begin of program
//       Handles creation, deletion, opening and closing of files.
//       It is associated with a PF_BufferMgr that manages the page
//       buffer and executes the page replacement policies.
//
PF_Manager::PF_Manager()
{
    // Create Buffer Manager
    pBufferMgr = new PF_BufferMgr(PF_BUFFER_SIZE);
}

//
// ~PF_Manager
//
// Desc: Destructor - intended to be called once at end of program
//       Destroys the buffer manager.
//       All files are expected to be closed when this method is called.
//
PF_Manager::~PF_Manager()
{
    // Destroy the buffer manager objects
    delete pBufferMgr;
}

//
// CreateFile
//
// Desc: 根据文件名在系统中创建一个文件，并注册文件描述符
//       将文件头页的页头写入磁盘文件
// In:   fileName - name of file to create
// Ret:  PF return code
//
RC PF_Manager::CreateFile (const char *fileName)
{
    int fd;		// unix file descriptor
    int numBytes;		// return code form write syscall

    // Create file for exclusive use
    if ((fd = open(fileName,
#ifdef PC
            O_BINARY |
#endif
                   O_CREAT | O_EXCL | O_WRONLY,
                   CREATION_MASK)) < 0)
        return (PF_UNIX);

    // Initialize the file header: must reserve FileHdrSize bytes in memory
    // though the actual size of FileHdr is smaller
    // char hdrBuf[PF_PAGE_SIZE];
    char hdrBuf[PF_FILE_HDR_SIZE];

    // So that Purify doesn't complain
    memset(hdrBuf, 0, PF_FILE_HDR_SIZE);

    PF_FileHdr *hdr = (PF_FileHdr*)hdrBuf;
    hdr->firstFree = PF_PAGE_LIST_END;
    hdr->numPages = 0;

    // Write header to file
    // 从页的数据项开始写内容
    // long offset = sizeof(PF_PageHdr);
    long offset = 0;
    if (lseek(fd, offset, L_SET) < 0)
        return (PF_UNIX);
    if((numBytes = write(fd, hdrBuf, PF_FILE_HDR_SIZE))
       != PF_FILE_HDR_SIZE) {

        // Error while writing: close and remove file
        close(fd);
        unlink(fileName);

        // Return an error
        if(numBytes < 0)
            return (PF_UNIX);
        else
            return (PF_HDRWRITE);
    }

    // Close file
    if(close(fd) < 0)
        return (PF_UNIX);

    // Return ok
    return (0);
}

//
// DestroyFile
//
// Desc: 销毁一个文件
// In:   fileName - name of file to delete
// Ret:  PF return code
//
RC PF_Manager::DestroyFile (const char *fileName)
{
    // Remove the file
    if (unlink(fileName) < 0)
        return (PF_UNIX);

    // Return ok
    return (0);
}

/**
 * 打开文件，将文件头页加载进内存
 * 1. 检查文件是否已经被打开，被打开的文件不能再次打开
 * 2. 调用open函数得到目标文件的文件描述符
 * 3. 从磁盘中将文件头页加载进内存
 * 4. fileHandle相关字段进行配置，同时*hdr指向内存的中的位置
 * @param fileName
 * @param fileHandle
 * @return
 */
RC PF_Manager::OpenFile (const char *fileName, PF_FileHandle &fileHandle)
{
    int rc;                   // return code

    if (fileHandle.bFileOpen)
        return (PF_FILEOPEN);

    // Open the file
    if ((fileHandle.unixfd = open(fileName,
#ifdef PC
            O_BINARY |
#endif
                                  O_RDWR)) < 0)
        return (PF_UNIX);


//    // 将文件头页加载进内存
//    char *hdrFile = (char*)fileHandle.hdr;           // hdrFile == fileHandle.hdr
//    if((rc =  pBufferMgr->GetPage(fileHandle.unixfd,PF_FILE_HDR_PAGENUM,&hdrFile))){
//        goto err;
//    }
    // 将文件头页加载进内存
    char *hdrFile = NULL;
    if((rc =  pBufferMgr->GetPage(fileHandle.unixfd,PF_FILE_HDR_PAGENUM,&hdrFile))){
        goto err;
    }
    // 上面的代码执行之后，hdr将指向缓冲区中的地址
    // fileHandle.hdr将指向缓冲区
    fileHandle.hdr = (PF_FileHdr *)hdrFile;

    // Set file header to be not changed
    fileHandle.bHdrChanged = FALSE;

    // Set local variables in file handle object to refer to open file
    fileHandle.pBufferMgr = pBufferMgr;
    fileHandle.bFileOpen = TRUE;

    // Return ok
    return OK_RC;

    err:
    // 打开文件失败
    // Close file
    fileHandle.UnpinPage(PF_FILE_HDR_PAGENUM);
    close(fileHandle.unixfd);
    fileHandle.bFileOpen = FALSE;

    return rc;
}

/**
 * 关闭文件
 * 1.检查文件是否已经关闭
 * 2.调用FlushPages方法将所有页回写磁盘
 * 3.调用close方法关闭文件
 * 4.设置文件状态值
 * @param fileHandle
 * @return
 */
RC PF_Manager::CloseFile(PF_FileHandle &fileHandle)
{
    RC rc;
    // 检查
    if (!fileHandle.bFileOpen)
        return (PF_CLOSEDFILE);
    // 由于OpenFile时，会一直保持对文件头页的pin,所以关闭文件
    // 将所有页回写之前，要将文件头页unpin
    if((rc = fileHandle.UnpinPage(PF_FILE_HDR_PAGENUM)))
        return rc;
    // 调用方法将所有页回写磁盘
    if ((rc = fileHandle.FlushPages()))
        return (rc);
    // 调用close方法关闭文件
    if (close(fileHandle.unixfd) < 0)
        return (PF_UNIX);

    // 重置标志位
    fileHandle.bFileOpen = FALSE;
    fileHandle.pBufferMgr = NULL;
    fileHandle.hdr = NULL;

    return OK_RC;
}

//
// ClearBuffer
//
// Desc: Remove all entries from the buffer manager.
//       This routine will be called via the system command and is only
//       really useful if the user wants to run some performance
//       comparison starting with an clean buffer.
// In:   Nothing
// Out:  Nothing
// Ret:  Returns the result of PF_BufferMgr::ClearBuffer
//       It is a code: 0 for success, something else for a PF error.
//
RC PF_Manager::ClearBuffer()
{
    return pBufferMgr->ClearBuffer();
}

//
// PrintBuffer
//
// Desc: Display all of the pages within the buffer.
//       This routine will be called via the system command.
// In:   Nothing
// Out:  Nothing
// Ret:  Returns the result of PF_BufferMgr::PrintBuffer
//       It is a code: 0 for success, something else for a PF error.
//
RC PF_Manager::PrintBuffer()
{
    return pBufferMgr->PrintBuffer();
}

//
// ResizeBuffer
//
// Desc: Resizes the buffer manager to the size passed in.
//       This routine will be called via the system command.
// In:   The new buffer size
// Out:  Nothing
// Ret:  Returns the result of PF_BufferMgr::ResizeBuffer
//       It is a code: 0 for success, PF_TOOSMALL when iNewSize
//       would be too small.
//
RC PF_Manager::ResizeBuffer(int iNewSize)
{
    return pBufferMgr->ResizeBuffer(iNewSize);
}

//------------------------------------------------------------------------------
// Three Methods for manipulating raw memory buffers.  These memory
// locations are handled by the buffer manager, but are not
// associated with a particular file.  These should be used if you
// want memory that is bounded by the size of the buffer pool.
//
// The PF_Manager just passes the calls down to the Buffer manager.
//------------------------------------------------------------------------------

RC PF_Manager::GetBlockSize(int &length) const
{
    return pBufferMgr->GetBlockSize(length);
}

RC PF_Manager::AllocateBlock(char *&buffer)
{
    return pBufferMgr->AllocateBlock(buffer);
}

RC PF_Manager::DisposeBlock(char *buffer)
{
    return pBufferMgr->DisposeBlock(buffer);
}
