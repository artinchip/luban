// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */
#include <common.h>
#include <artinchip/aicupg.h>
#include "upg_internal.h"

#if 0
#undef debug
#define debug printf
#endif

struct artinchip_image_info {
	char magic[8];
	char platform[64];
	char product[64];
	char version[64];
	char media_type[64];
	u32  media_dev_id;
	u8   media_id[64]; /* NAND ID or MMC ID */
	u32  meta_offset; /* Meta Area start offset */
	u32  meta_size; /* Meta Area size */
	u32  file_offset; /* File data Area start offset */
	u32  file_size; /* File data Area size */
};

static struct artinchip_image_info image_info;
void ram_fwc_start(struct fwc_info *fwc)
{
	debug("%s, FWC name %s\n", __func__, fwc->meta.name);
	if (strcmp(fwc->meta.name, "image.info") != 0) {
		fwc->priv = 0; /* Don't process it */
	} else {
		fwc->priv = &image_info;
		memset(&image_info, 0, sizeof(image_info));
	}
	fwc->block_size = 1;
}

static enum upg_dev_type get_device_type(struct artinchip_image_info *info)
{
	enum upg_dev_type type = UPG_DEV_TYPE_RAM;

	debug("%s, %s\n", __func__, info->media_type);
	if (strcmp(info->media_type, "mmc") == 0)
		type = UPG_DEV_TYPE_MMC;
	else if (strcmp(info->media_type, "raw-nand") == 0)
		type = UPG_DEV_TYPE_RAW_NAND;
	else if (strcmp(info->media_type, "spi-nand") == 0)
		type = UPG_DEV_TYPE_SPI_NAND;
	else if (strcmp(info->media_type, "spi-nor") == 0)
		type = UPG_DEV_TYPE_SPI_NOR;

	return type;
}

enum upg_dev_type get_media_type(struct fwc_info *fwc)
{
	enum upg_dev_type type = UPG_DEV_TYPE_RAM;

	debug("%s, %s\n", __func__, fwc->mpart.media.media_type);
	if (strcmp(fwc->mpart.media.media_type, "mmc") == 0)
		type = UPG_DEV_TYPE_MMC;
	else if (strcmp(fwc->mpart.media.media_type, "raw-nand") == 0)
		type = UPG_DEV_TYPE_RAW_NAND;
	else if (strcmp(fwc->mpart.media.media_type, "spi-nand") == 0)
		type = UPG_DEV_TYPE_SPI_NAND;
	else if (strcmp(fwc->mpart.media.media_type, "spi-nor") == 0)
		type = UPG_DEV_TYPE_SPI_NOR;

	return type;
}

static s32 image_info_proc(struct fwc_info *fwc,
			   struct artinchip_image_info *info)
{
	enum upg_dev_type type;
	s32 ret = 0;

	type = get_device_type(info);
	pr_info("Write to: %s(%d)\n", get_current_device_name(type), type);
#ifdef CONFIG_AICUPG_MMC_ARTINCHIP
	if (type == UPG_DEV_TYPE_MMC) {
		/*
		 * STEP1: make GPT partitions
		 * If create GPT partitions failed, should RESP failure to HOST
		 */
		ret = mmc_fwc_prepare(fwc, info->media_dev_id);
		if (ret < 0) {
			pr_err("Create GPT partitions failed\n");
			return ret;
		}
	}
#endif
#ifdef CONFIG_AICUPG_NAND_ARTINCHIP
	if (type == UPG_DEV_TYPE_SPI_NAND) {
		/*
		 * When receive FWC: image info, it is going to start firmware
		 * upgrading, probe mtd device and create ubi volumes first.
		 */
		if (get_nand_prepare_status() != true) {
			ret = nand_fwc_prepare(fwc, info->media_dev_id);
			if (ret < 0) {
				pr_err("NAND prepare failed\n");
				return ret;
			}
		}
	}
#endif
#ifdef CONFIG_AICUPG_NOR_ARTINCHIP
	if (type == UPG_DEV_TYPE_SPI_NOR) {
		/*
		 * When receive FWC: image info, it is going to start firmware
		 * upgrading, probe mtd device first.
		 */
		ret = nor_fwc_prepare(fwc, info->media_dev_id);
		if (ret < 0) {
			pr_err("NOR prepare failed\n");
			return ret;
		}
	}
#endif

	return 0;
}

/*
 * Current implementation, only one special RAM FWC need to be processed,
 *     image.info
 * This FWC is the same with fiwmware image header, and we can get information
 * about media type which firmware will be burn.
 *
 * This FWC will be sent before any FWC in "target" is coming.
 */
s32 ram_fwc_data_write(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct artinchip_image_info *info;

	fwc->trans_size += len;
	fwc->calc_partition_crc = crc32(fwc->calc_partition_crc, buf, len);

	debug("%s, data len %d, trans len %d\n", __func__, len, fwc->trans_size);

	if (fwc->priv == 0)
		return len;

	/* Only process image info data */
	if (len >= sizeof(*info)) {
		info = (struct artinchip_image_info *)fwc->priv;
		memcpy(info, buf, sizeof(*info));
		image_info_proc(fwc, info);
	}

	pr_debug("%s, l: %d\n", __func__, __LINE__);
	return len;
}

void ram_fwc_data_end(struct fwc_info *fwc)
{
	struct artinchip_image_info *info;
	enum upg_dev_type type;

	if (fwc->priv) {
		info = (struct artinchip_image_info *)fwc->priv;
		type = get_device_type(info);

		/* Switch device type for following FWCs */
		set_current_device_type(type);
		set_current_device_id(info->media_dev_id);
	}
}
