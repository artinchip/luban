/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Dehuang Wu <dehuang.wu@artinchip.com>
 */

#ifndef __AICUPG_NAND_FWC_PRIV_H__
#define __AICUPG_NAND_FWC_PRIV_H__

#include <linux/kernel.h>
#include <linux/sizes.h>
#include <mtd.h>
#include <nand.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include "upg_internal.h"

#define MAX_DUPLICATED_PART      4

struct aicupg_nand_priv {
	int attr;
	/* MTD partitions FWC will be written to */
	struct mtd_info *mtds[MAX_DUPLICATED_PART];
	loff_t offs[MAX_DUPLICATED_PART];
	/* UBI Volumes FWC will be written to */
	struct ubi_volume *vols[MAX_DUPLICATED_PART];
	int vol_leb_num[MAX_DUPLICATED_PART];
	bool spl_flag;
};


#endif /* __AICUPG_NAND_FWC_PRIV_H__ */
