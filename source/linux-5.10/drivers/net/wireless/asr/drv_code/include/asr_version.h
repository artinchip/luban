/**
 ******************************************************************************
 *
 * @file asr_version.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_VERSION_H_
#define _ASR_VERSION_H_

#define _MACROSTR(x) #x
#define MACROSTR(x) _MACROSTR(x)

#define FW_BUILD_MACHINE MACROSTR(CFG_BUILD_MACHINE)
#define FW_BUILD_DATE MACROSTR(CFG_BUILD_DATE)
#define ASR_VERS_NUM "v5.5.3.0"

static inline void asr_print_version(void)
{
	pr_info("ASR wireless: Driver version: %s_%s@%s\n", ASR_VERS_NUM, FW_BUILD_DATE, FW_BUILD_MACHINE);
}

#endif /* _ASR_VERSION_H_ */
