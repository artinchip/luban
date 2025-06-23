// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <image.h>
#include <artinchip_crypto.h>

int aic_aes_cbc_decrypt(struct image_cipher_info *info, const void *cipher,
			size_t cipher_len, void **data, size_t *size)
{
	struct udevice *dev = NULL;
	int ret;

	ret = crypto_init_device(&dev);
	if (ret)
		return ret;

	if (dev == NULL)
		return -ENODEV;

	*data = malloc(cipher_len);
	if (!*data) {
		printf("Can't allocate memory to decrypt\n");
		return -ENOMEM;
	}
	*size = info->size_unciphered;

	ret = aes_init(dev);
	if (ret)
		return ret;

	ret = aes_set_decrypt_key(dev, (void *)info->key, info->cipher->key_len);
	if (ret) {
		printf("AES set key failed.\n");
		return ret;
	}

	ret = aes_cbc_decrypt(dev, (void *)cipher, *data, cipher_len, (void *)info->iv);
	if (ret) {
		printf("AES decrypt failed.\n");
		return ret;
	}

	aes_exit(dev);

	return ret;
}

