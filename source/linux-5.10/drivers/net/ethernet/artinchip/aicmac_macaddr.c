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
	struct crypto_ahash *tfm;
	struct ahash_request *req;
	struct scatterlist sg;
	unsigned char result[MAC_MD5_SIZE] = { 0 };
	int i = 0;
	int ret = -1;

	tfm = crypto_alloc_ahash("md5", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm)) {
		pr_err("Failed to alloc md5\n");
		return ret;
	}

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req)
		goto out;

	ahash_request_set_callback(req, 0, NULL, NULL);
	ret = crypto_ahash_init(req);
	if (ret) {
		pr_err("crypto_ahash_init() failed\n");
		goto out;
	}

	sg_init_one(&sg, chipid, AIC_CHIPID_SIZE);
	ahash_request_set_crypt(req, &sg, result, sizeof(chipid) - 1);
	ret = crypto_ahash_update(req);
	if (ret) {
		pr_err("crypto_ahash_update() failed for id\n");
		goto out;
	}

	ret = crypto_ahash_final(req);
	if (ret) {
		pr_err("crypto_ahash_final() failed for result\n");
		goto out;
	}

	ahash_request_free(req);
	/* Choose md5 result's [0][2][4][6][8][10] byte as mac address */
	for (i = 0; i < ETH_ALEN; i++)
		addr[i] = result[2 * i];

	ret = 0;
out:
	crypto_free_ahash(tfm);

	return ret;
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
