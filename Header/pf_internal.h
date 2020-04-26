// File:        pf_internal.h
// Description: Declarations internal to the paged file component


#ifndef PF_INTERNAL_H
#define PF_INTERNAL_H

#include <cstdlib>
#include <cstring>
#include "pf.h"

//
// Constants and defines
//
const int PF_BUFFER_SIZE = 40;     // Number of pages in the buffer
const int PF_HASH_TBL_SIZE = 20;   // Size of hash table

#define CREATION_MASK      0600    // r/w privileges to owner only


// L_SET is used to indicate the "whence" argument of the lseek call
// defined in "/usr/include/unistd.h".  A value of 0 indicates to
// move to the absolute location specified.
#ifndef L_SET
#define L_SET              0
#endif


// 文件头也占用一个页的空间
const int PF_FILE_HDR_SIZE = PF_PAGE_SIZE + sizeof(PF_PageHdr);

#endif
