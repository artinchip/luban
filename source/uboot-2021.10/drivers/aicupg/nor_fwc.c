// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021-2025 ArtInChip Technology Co., Ltd
 * Author: Jianfeng Li <jianfeng.li@artinchip.com>
 */
#include <common.h>
#include <env.h>
#include <artinchip/aicupg.h>
#include "upg_internal.h"
#include <linux/mtd/mtd.h>
#include <spi.h>
#include <spi_flash.h>
#include <dm/device-internal.h>
#include <mtd.h>
#include <artinchip/aic_spienc.h>

#include "spi_enc_spl.h"

#define MAX_DUPLICATED_PART      6
#define MAX_NOR_NAME            32

struct aicupg_nor_priv {
	/* MTD partitions FWC will be written to */
	struct mtd_info *mtds[MAX_DUPLICATED_PART];
	loff_t offs[MAX_DUPLICATED_PART];
	uint64_t erased_address[MAX_DUPLICATED_PART];
};

static bool is_all_ff(u8 *buf, s32 len)
{
	for (int i = 0; i < len; i++) {
		if (buf[i] != 0xFF) {
			return false;
		}
	}
	return true;
}

/*
 * Erese one sector
 */
static s32 do_mtd_erase_sector(struct mtd_info *mtd, uint64_t address)
{
	struct erase_info erase_op = {};
	s32 ret;

	ret = 0;
	erase_op.mtd = mtd;
	erase_op.addr = address;
	erase_op.len = mtd->erasesize;
	erase_op.scrub = false;
	ret = mtd_erase(mtd, &erase_op);
	if (ret) {
		pr_err("do mtd erase failed.\n");
	}
	return ret;
}

static s32 nor_fwc_get_mtd_partitions(struct fwc_info *fwc,
			struct aicupg_nor_priv *priv)
{
	char name[MAX_NOR_NAME], *p;
	int cnt, idx;
	struct mtd_info *mtd;

	p = fwc->meta.partition;
	cnt = 0;
	idx = 0;
	while (*p) {
		if (cnt >= MAX_NOR_NAME) {
			pr_err("Partition name is too long.\n");
			return -1;
		}

		name[cnt] = *p;
		p++;
		cnt++;
		if (*p == ';' || *p == '\0') {
			name[cnt] = '\0';
			mtd = get_mtd_device_nm(name);
			/*Erase address init value is 0x0*/
			if (IS_ERR_OR_NULL(mtd)) {
				pr_err("Get mtd %s failed.\n", name);
				return -1;
			}
			priv->erased_address[idx] = 0x0;
			priv->mtds[idx] = mtd;
			idx++;
			cnt = 0;
		}
		if (*p == ';')
			p++;
		if (*p == '\0')
			break;
	}
	return 0;
}

s32 nor_is_exist(void)
{
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	/* In DM mode, defaults speed and mode will be taken from DT */
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;
	struct spi_flash *new;

	new = spi_flash_probe(bus, cs, speed, mode);
	if (!new) {
		pr_err("Failed to initialize SPI flash at %u:%u\n", bus, cs);
		return false;
	}
	return true;
}

/*
 * storage device prepare,should probe spi norflash
 *  - probe spi device
 *  - set env for MTD partitions
 *  - probe MTD device
 */
s32 nor_fwc_prepare(struct fwc_info *fwc, u32 id)
{
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	/* In DM mode, defaults speed and mode will be taken from DT */
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;
	char *mtdparts;
	s32 ret = 0;
#ifdef CONFIG_DM_SPI_FLASH
	struct udevice *new, *bus_dev;
#else
	struct spi_flash *new;
#endif

#ifdef CONFIG_DM_SPI_FLASH
	/* Remove the old device, otherwise probe will just be a nop */
	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (!ret) {
		device_remove(new, DM_REMOVE_NORMAL);
	}

	ret = spi_flash_probe_bus_cs(bus, cs, speed, mode, &new);
	if (ret) {
		pr_err("Failed to initialize SPI flash at %u:%u (error %d)\n",
			   bus, cs, ret);
		goto err;
	}
#else
	new = spi_flash_probe(bus, cs, speed, mode);
	if (!new) {
		pr_err("Failed to initialize SPI flash at %u:%u\n", bus, cs);
		goto err;
	}
#endif

	mtdparts = env_get("MTD");
	if (!mtdparts) {
		pr_err("Get MTD partition table from ENV failed.\n");
		return -ENODEV;
	}
	env_set("mtdparts", mtdparts);

	ret = mtd_probe_devices();
	if (ret) {
		pr_err("mtd probe partitions failed.\n");
		goto err;
	}
	return ret;
err:
	return ret;
}

/*
 * New FirmWare Component start, should prepare to burn FWC to NOR
 *  - Get FWC attributes
 *  - Parse MTD partitions  FWC going to upgrade
 */
void nor_fwc_start(struct fwc_info *fwc)
{
	struct aicupg_nor_priv *priv;
	struct mtd_info *mtd;
	int ret = 0;

	priv = malloc(sizeof(struct  aicupg_nor_priv));
	if (!priv) {
		pr_err("Out of memory, malloc failed.\n");
		goto err;
	}
	memset(priv, 0, sizeof(struct aicupg_nor_priv));
	fwc->priv = priv;
	/*get the MTD partitions for prepare write*/
	ret = nor_fwc_get_mtd_partitions(fwc, priv);
	if (ret) {
		pr_err("Get MTD partitions failed.\n");
		goto err;
	}
	mtd = priv->mtds[0];
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("MTD device is not found.\n");
		goto err;
	}

#ifdef CONFIG_ARTINCHIP_SPIENC
	if (strstr(fwc->meta.name, "target.spl"))
		spi_enc_tweak_select(AIC_SPIENC_HW_TWEAK);
#endif
	fwc->block_size = mtd->writesize;
	debug("%s, FWC name %s\n", __func__, fwc->meta.name);
	return;
err:
	pr_err("error:free(priv)\n");
	free(priv);
	fwc->priv = NULL;
}

/*
 * New FirmWare Component write, should write data to NOR
 *  - Erase MTD partitions for prepare write
 *  - Write data to MTD partions
 */
s32 nor_fwc_data_write(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_nor_priv *priv;
	struct mtd_info *mtd;
	s32 offset = 0;
	uint64_t erased_addr = 0;
	s32 i = 0, calc_len;
	s32 ret = 0;
	size_t retlen;
	u8 *rdbuf;

	priv = (struct aicupg_nor_priv *)fwc->priv;
	if (!priv)
		return 0;
	rdbuf = valloc(len);
	for (i = 0; i < MAX_DUPLICATED_PART; i++) {
		mtd = priv->mtds[i];
		if (!mtd)
			continue;
		offset = priv->offs[i];
		erased_addr = priv->erased_address[i];
		if ((offset + len) > mtd->size) {
			pr_err("Not enough space to write mtd %s\n", mtd->name);
			return 0;
		}
		/* erase 1 sector when offset+len more than erased address */
		while ((offset + len) > erased_addr) {
			ret = mtd_read(mtd, erased_addr, mtd->erasesize, &retlen, rdbuf);
			if (ret) {
				pr_err("Read mtd %s error.\n", mtd->name);
				return 0;
			}

			if (!is_all_ff(rdbuf, len)) {
				ret = do_mtd_erase_sector(mtd, erased_addr);
				if (ret) {
					pr_err("Mtd erse sector failed!..\n");
					return 0;
				}
			}
			/*Update for next erase*/
			priv->erased_address[i] = erased_addr + mtd->erasesize;
			erased_addr = priv->erased_address[i];
		}
		if (!is_all_ff(buf, len)) {
			ret = mtd_write(mtd, offset, len, &retlen, buf);
			if (ret) {
				pr_err("Write mtd %s error.\n", mtd->name);
				return 0;
			}
		}

		/* Update for next write */
		// Read data to calc crc
		ret = mtd_read(mtd, offset, len, &retlen, rdbuf);
		if (ret) {
			pr_err("Read mtd %s to calc crc error.\n", mtd->name);
			return 0;
		}
		priv->offs[i] = offset + retlen;
	}

	if ((fwc->meta.size - fwc->trans_size) < len)
		calc_len = fwc->meta.size - fwc->trans_size;
	else
		calc_len = len;

	fwc->calc_partition_crc = crc32(fwc->calc_partition_crc, rdbuf, calc_len);
#ifdef CONFIG_AICUPG_SINGLE_TRANS_BURN_CRC32_VERIFY
	fwc->calc_trans_crc = crc32(fwc->calc_trans_crc, buf, calc_len);
	if (fwc->calc_trans_crc != fwc->calc_partition_crc) {
		pr_err("calc_len:%d\n", calc_len);
		pr_err("crc err at trans len %u\n", fwc->trans_size);
		pr_err("trans crc:0x%x, partition crc:0x%x\n",
				fwc->calc_trans_crc, fwc->calc_partition_crc);
	}
#endif
	fwc->trans_size += len;
	debug("%s, data len %d, trans len %d\n", __func__, len, fwc->trans_size);

	free(rdbuf);
	return len;
}

s32 nor_fwc_data_read(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_nor_priv *priv;
	struct mtd_info *mtd;
	s32 offset = 0;
	s32 ret = 0;
	size_t retlen;

	priv = (struct aicupg_nor_priv *)fwc->priv;
	if (!priv)
		return 0;
	mtd = priv->mtds[0];
	if (!mtd)
		return 0;

	offset = priv->offs[0];
	ret = mtd_read(mtd, offset, len, &retlen, buf);
	if (ret) {
		pr_err("read mtd %s error.\n", mtd->name);
		return 0;
	}
	priv->offs[0] = offset + retlen;

	fwc->trans_size += len;
	fwc->calc_partition_crc = fwc->meta.crc;
	debug("%s, data len %d, trans len %d\n", __func__, len, fwc->trans_size);

	return len;
}

/*
 * New FirmWare Component end, should free memory
 *  - free the memory what to  deposit device information
 */
void nor_fwc_data_end(struct fwc_info *fwc)
{
	struct aicupg_nor_priv *priv;

	priv = (struct aicupg_nor_priv *)fwc->priv;
	if (!priv)
		return;
#ifdef CONFIG_ARTINCHIP_SPIENC
	if (strstr(fwc->meta.name, "target.spl"))
		spi_enc_tweak_select(AIC_SPIENC_USER_TWEAK);
#endif
	debug("trans len all %d\n", fwc->trans_size);
	free(priv);
}
