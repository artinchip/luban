// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd
 */

#include <common.h>
#include <asm/global_data.h>
#include <mmc.h>
#include <part.h>
#include <asm/arch/boot_param.h>

DECLARE_GLOBAL_DATA_PTR;

int mmc_get_env_dev(void)
{
	enum boot_device bd;
	int devno = CONFIG_SYS_MMC_ENV_DEV;

	bd = aic_get_boot_device();
	switch (bd) {
	case BD_SDMC0:
		devno = 0;
		break;
	case BD_SDMC1:
		devno = 1;
		break;
	default:
		devno = CONFIG_SYS_MMC_ENV_DEV;
		break;
	}

	return devno;
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
static inline int aic_mmc_offset_try_partition(const char *str, int copy,
					       s64 *val)
{
	struct disk_partition info;
	struct blk_desc *desc;
	int len, i, ret;
	char dev_str[4];

	snprintf(dev_str, sizeof(dev_str), "%d", mmc_get_env_dev());
	ret = blk_get_device_by_str("mmc", dev_str, &desc);
	if (ret < 0)
		return (ret);

	for (i = 1; ; i++) {
		ret = part_get_info(desc, i, &info);
		if (ret < 0)
			return ret;

		if (!strncmp((const char *)info.name, str, sizeof(str)))
			break;
	}

	/* round up to info.blksz */
	len = DIV_ROUND_UP(CONFIG_ENV_SIZE, info.blksz);

	/* use the top of the partion for the environment */
	*val = (info.start + copy * len) * info.blksz;

	return 0;
}

static inline s64 aic_mmc_offset(int copy)
{
	const struct {
		const char *offset_redund;
		const char *partition;
		const char *offset;
	} dt_prop = {
		.offset_redund = "u-boot,mmc-env-offset-redundant",
		.partition = "u-boot,mmc-env-partition",
		.offset = "u-boot,mmc-env-offset",
	};
	s64 val = 0, defvalue;
	const char *propname;
	const char *str;
	int err;

	str = fdtdec_get_config_string(gd->fdt_blob, dt_prop.partition);
	if (str) {
		/*
		 * AIC put env in 'env' partition, read from start to end.
		 */
		err = aic_mmc_offset_try_partition(str, copy, &val);
		if (!err)
			return val;
	}

	defvalue = CONFIG_ENV_OFFSET;
	propname = dt_prop.offset;

#if defined(CONFIG_ENV_OFFSET_REDUND)
	if (copy) {
		defvalue = CONFIG_ENV_OFFSET_REDUND;
		propname = dt_prop.offset_redund;
	}
#endif
	return fdtdec_get_config_int(gd->fdt_blob, propname, defvalue);
}

#else // !CONFIG_IS_ENABLED(OF_CONTROL)
static inline s64 aic_mmc_offset(int copy)
{
	s64 offset = CONFIG_ENV_OFFSET;

#if defined(CONFIG_ENV_OFFSET_REDUND)
	if (copy)
		offset = CONFIG_ENV_OFFSET_REDUND;
#endif
	return offset;
}
#endif

int mmc_get_env_addr(struct mmc *mmc, int copy, u32 *env_addr)
{
	s64 offset = aic_mmc_offset(copy);

	if (offset < 0)
		offset += mmc->capacity;

	*env_addr = offset;

	return 0;
}
