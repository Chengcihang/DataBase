//
//  SM_PrintError.h
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/28.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#ifndef SM_PrintError_h
#define SM_PrintError_h

//打印错误功能
void SM_PrintError(RC rc);

#define SM_CANNOTCLOSE          (START_SM_WARN + 0) //无效的RID
#define SM_BADRELNAME           (START_SM_WARN + 1)
#define SM_BADREL               (START_SM_WARN + 2)
#define SM_BADATTR              (START_SM_WARN + 3)
#define SM_INVALIDATTR          (START_SM_WARN + 4)
#define SM_INDEXEDALREADY       (START_SM_WARN + 5)
#define SM_NOINDEX              (START_SM_WARN + 6)
#define SM_BADLOADFILE          (START_SM_WARN + 7)
#define SM_BADSET               (START_SM_WARN + 8)
#define SM_LASTWARN             SM_BADSET

#define SM_INVALIDDB            (START_SM_ERR - 0)
#define SM_ERROR                (START_SM_ERR - 1) //错误
#define SM_LASTERROR            SM_ERROR


#endif /* SM_PrintError_h */
