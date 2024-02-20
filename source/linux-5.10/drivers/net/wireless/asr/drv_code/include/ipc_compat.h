/**
 ****************************************************************************************
 *
 * @file ipc_compat.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _IPC_COMPAT_H_
#define _IPC_H_

#define __INLINE static __attribute__((__always_inline__)) inline

#define __ALIGN4 __aligned(4)

#define ASSERT_ERR(condition)                                                           \
    do {                                                                                \
        if (unlikely(!(condition))) {                                                  \
            pr_err("%s:%d:ASSERT_ERR(" #condition ")\n", __FILE__,  __LINE__);          \
        }                                                                               \
    } while(0)

#endif /* _IPC_COMPAT_H_ */
