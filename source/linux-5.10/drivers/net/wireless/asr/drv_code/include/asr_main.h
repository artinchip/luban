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

struct asr_vendor_data
{
    //0 reg write
    //1 reg read
    //2 rfreg write
    //3 rfreg read 
    //4 set cca
    //5 get cca
    //6 wifi_recv
    //7 wifi_crash
    //8 debug show
    //9 debug stop
    //a wifi dbg tx 
    //b wifi dbg rx
    //c wifi dbg ba
    //d wifi dbg sta
    //e wifi reset mib
    //f rfota mode 
    //10 agg disable 
    u32  cmd;
    u32  reg;
    u32  value;
};

enum asr_vndr_cmds {
	ASR_VNDR_CMDS_DCMD = 0x100,
	ASR_VNDR_CMDS_LAST
};

extern int driver_mode;

int asr_cfg80211_init(struct asr_plat *asr_plat, void **platform_data);
void asr_cfg80211_deinit(struct asr_hw *asr_hw);
#ifdef CONFIG_ASR_USB
int asr_usb_platform_init(struct asr_usbdev_info *asr_plat, void **platform_data);
#endif

#endif /* _ASR_MAIN_H_ */
