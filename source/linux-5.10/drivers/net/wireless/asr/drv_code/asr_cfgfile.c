/**
 ****************************************************************************************
 *
 * @file asr_configparse.c
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ****************************************************************************************
 */
#include <linux/firmware.h>
#include <net/mac80211.h>

#include "asr_defs.h"
#include "asr_cfgfile.h"

/**
 * Parse the Config file used at init time
 */
int asr_parse_phy_configfile(struct asr_hw *asr_hw, struct asr_phy_conf_file *config)
{
	/* Get Trident path mapping */
	config->trd.path_mapping = asr_hw->mod_params->phy_cfg;
	asr_dbg(INFO, "Trident path mapping is: %d\n", config->trd.path_mapping);

	/* Get DC offset compensation */
	config->trd.tx_dc_off_comp = 0;
	asr_dbg(INFO, "TX DC offset compensation is: %08X\n", (unsigned int)config->trd.tx_dc_off_comp);

	/* Get Karst TX IQ compensation value for path0 on 2.4GHz */
	config->karst.tx_iq_comp_2_4G[0] = 0x01000000;

	/* Get Karst TX IQ compensation value for path1 on 2.4GHz */
	config->karst.tx_iq_comp_2_4G[1] = 0x01000000;

	/* Get Karst RX IQ compensation value for path0 on 2.4GHz */
	config->karst.rx_iq_comp_2_4G[0] = 0x01000000;

	/* Get Karst RX IQ compensation value for path1 on 2.4GHz */
	config->karst.rx_iq_comp_2_4G[1] = 0x01000000;

	asr_dbg(INFO,
		"Karst TX IQ compensation for path 0 on 2.4GHz is: %08X\n",
		(unsigned int)config->karst.tx_iq_comp_2_4G[0]);
	asr_dbg(INFO,
		"Karst TX IQ compensation for path 1 on 2.4GHz is: %08X\n",
		(unsigned int)config->karst.tx_iq_comp_2_4G[1]);
	asr_dbg(INFO,
		"Karst RX IQ compensation for path 0 on 2.4GHz is: %08X\n",
		(unsigned int)config->karst.rx_iq_comp_2_4G[0]);
	asr_dbg(INFO,
		"Karst RX IQ compensation for path 1 on 2.4GHz is: %08X\n",
		(unsigned int)config->karst.rx_iq_comp_2_4G[1]);
	/* Get Karst default path */
	config->karst.path_used = asr_hw->mod_params->phy_cfg;

	asr_dbg(INFO, "Karst default path is: %d\n", config->karst.path_used);

	return 0;
}
