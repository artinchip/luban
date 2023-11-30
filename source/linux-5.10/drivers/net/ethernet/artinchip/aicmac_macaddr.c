// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <linux/random.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/etherdevice.h>
#include <linux/nvmem-consumer.h>

#include "aicmac_util.h"

#define AIC_CHIPID_SIZE 16
#define MAC_MD5_SIZE 16

static int aicmac_macaddr_md5_ahash(unsigned char *chipid, unsigned char *addr)
{
	struct shash_desc *desc;
	struct crypto_shash *shash = NULL;
	unsigned char result[MAC_MD5_SIZE] = { 0 };
	int i = 0;
	size_t size = 0;

	shash = crypto_alloc_shash("md5", 0, CRYPTO_ALG_ASYNC);
	size = sizeof(struct shash_desc) + crypto_shash_descsize(shash);
	desc = kmalloc(size, GFP_KERNEL);
	if (!desc) {
		crypto_free_shash(shash);
		return -1;
	}

	desc->tfm = shash;

	crypto_shash_init(desc);
	crypto_shash_update(desc, chipid, AIC_CHIPID_SIZE);
	crypto_shash_final(desc, result);

	/* Choose md5 result's [0][2][4][6][8][10] byte as mac address */
	for (i = 0; i < ETH_ALEN; i++)
		addr[i] = result[2 * i];

	crypto_free_shash(shash);
	kfree(desc);
	return 0;
}

static int aicmac_get_soc_chipid(struct device *dev, unsigned char *chipid)
{
	struct nvmem_cell *cell;
	size_t len;
	void *value;

	cell = devm_nvmem_cell_get(dev, "chipid");
	if (IS_ERR(cell)) {
		dev_info(dev, "Failed to get cell\n");
		return -1;
	}

	value = nvmem_cell_read(cell, &len);
	if (len == 0) {
		dev_info(dev, "Efuse didn't burn calibration value\n");
		return -1;
	}
	sprintf(chipid, "%llx", *((u64 *)value));
	return 0;
}

static void aicmac_macaddr_from_chipid(struct device *dev, int bus_id, unsigned char *addr)
{
	unsigned char chipid[17] = { 0 };
	int ret = -1;

	ret = aicmac_get_soc_chipid(dev, (unsigned char *)chipid);

	if (ret != 0) {
		pr_err("%s can't get chipid\n", __func__);
		return;
	}

	if (bus_id == 0)
		chipid[15] = 'a';
	else
		chipid[15] = 'A';

	ret = aicmac_macaddr_md5_ahash(chipid, addr);

	if (ret != 0) {
		pr_err("%s failed to md5 chipid\n", __func__);
		return;
	}
	addr[0] &= 0xfe; /* clear multicast bit */
	addr[0] |= 0x02; /* set local assignment bit (IEEE802) */
}

static void aicmac_macaddr_from_random(unsigned char *addr)
{
	get_random_bytes(addr, ETH_ALEN);
	addr[0] &= 0xfe; /* clear multicast bit */
	addr[0] |= 0x02; /* set local assignment bit (IEEE802) */
}

void aicmac_get_mac_addr(struct device *dev, int bus_id, unsigned char *addr)
{
	if (!is_valid_ether_addr(addr)) {
		aicmac_macaddr_from_chipid(dev, bus_id, addr);

		if (!is_valid_ether_addr(addr))
			aicmac_macaddr_from_random(addr);
	}
}
