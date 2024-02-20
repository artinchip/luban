/**
 ****************************************************************************************
 *
 * @file asr_cfgfile.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef _ASR_CFGFILE_H_
#define _ASR_CFGFILE_H_

//#define LOAD_MAC_ADDR_FROM_FW

/*
 * Structure used to retrieve information from the Config file used at Initialization time
 */
struct asr_conf_file {
	u8 mac_addr[ETH_ALEN];
};

/*
 * Structure used to retrieve information from the PHY Config file used at Initialization time
 */
struct asr_phy_conf_file {
	struct phy_trd_cfg_tag trd;
	struct phy_karst_cfg_tag karst;
};

int asr_parse_phy_configfile(struct asr_hw *asr_hw, struct asr_phy_conf_file *config);

#endif /* _ASR_CFGFILE_H_ */
