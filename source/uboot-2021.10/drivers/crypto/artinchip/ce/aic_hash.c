// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <hw_sha.h>
#include <artinchip_crypto.h>
#include <u-boot/sha1.h>
#include <u-boot/sha256.h>
#include <u-boot/sha512.h>

#ifdef CONFIG_SPL_BUILD
const uint8_t sha1_der_prefix[SHA1_DER_LEN] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
	0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14
};

const uint8_t sha256_der_prefix[SHA256_DER_LEN] = {
	0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	0x00, 0x04, 0x20
};

const uint8_t sha384_der_prefix[SHA384_DER_LEN] = {
	0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05,
	0x00, 0x04, 0x30
};

const uint8_t sha512_der_prefix[SHA512_DER_LEN] = {
	0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05,
	0x00, 0x04, 0x40
};
#endif

int hw_sha(const unsigned char *pbuf, unsigned int buf_len,
	    unsigned char *pout, int mode)
{
	struct udevice *dev = NULL;
	sha_context_t ctx;
	int ret, outlen;

	ret = crypto_init_device(&dev);
	if (ret)
		return ret;

	if (dev == NULL)
		return -ENODEV;

	ret = sha_init(dev);
	if (ret)
		return ret;

	ret = sha_start(dev, &ctx, mode);
	if (ret) {
		printf("SHA start failed.\n");
		return ret;
	}
	ret = sha_update(dev, &ctx, pbuf, buf_len, 1);
	if (ret) {
		printf("SHA update data failed.\n");
		return ret;
	}
	ret = sha_finish(dev, &ctx, pout, &outlen);
	if (ret) {
		printf("SHA finish data failed.\n");
		return ret;
	}

	sha_exit(dev);

	return ret;
}

void hw_sha256(const unsigned char *pbuf, unsigned int buf_len,
			unsigned char *pout, unsigned int chunk_size)
{
	if (hw_sha(pbuf, buf_len, pout, SHA_MODE_256))
		printf("calc sha finish\n");
}

void hw_sha1(const unsigned char *pbuf, unsigned int buf_len,
			unsigned char *pout, unsigned int chunk_size)
{
	if (hw_sha(pbuf, buf_len, pout, SHA_MODE_1))
		printf("calc sha finish\n");
}

int hw_sha_init(struct hash_algo *algo, void **ctxp)
{
	struct udevice *dev = NULL;
	*ctxp = malloc(sizeof(sha_context_t));
	int ret;

	ret = crypto_init_device(&dev);
	if (ret)
		return ret;

	if (dev == NULL)
		return -ENODEV;

	ret = sha_init(dev);
	if (ret)
		return ret;

	ret = sha_start(dev, *ctxp, SHA_MODE_256);
	if (ret) {
		printf("SHA start failed.\n");
		return ret;
	}

	return ret;
}

int hw_sha_update(struct hash_algo *algo, void *ctx, const void *buf,
			    unsigned int size, int is_last)
{
	struct udevice *dev = NULL;
	int ret;

	ret = crypto_init_device(&dev);
	if (ret)
		return ret;

	if (dev == NULL)
		return -ENODEV;

	ret = sha_update(dev, ctx, buf, size, is_last);
	if (ret) {
		printf("SHA update data failed.\n");
		return ret;
	}
	return ret;
}

int hw_sha_finish(struct hash_algo *algo, void *ctx, void *dest_buf,
		     int size)
{
	struct udevice *dev;
	int ret, outlen;

	ret = crypto_init_device(&dev);
	if (ret)
		return ret;

	ret = sha_finish(dev, ctx, dest_buf, &outlen);
	if (ret) {
		printf("SHA finish data failed.\n");
		return ret;
	}

	sha_exit(dev);

	free(ctx);
	return ret;
}

