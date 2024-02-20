/**
 ******************************************************************************
 *
 * @file asr_main.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_MAIN_H_
#define _ASR_MAIN_H_

#include "asr_defs.h"
#ifdef CONFIG_ASR_USB
#include "asr_usb.h"
#endif

extern int driver_mode;

int asr_cfg80211_init(struct asr_plat *asr_plat, void **platform_data);
void asr_cfg80211_deinit(struct asr_hw *asr_hw);
#ifdef CONFIG_ASR_USB
int asr_usb_platform_init(struct asr_usbdev_info *asr_plat, void **platform_data);
#endif

#endif /* _ASR_MAIN_H_ */
