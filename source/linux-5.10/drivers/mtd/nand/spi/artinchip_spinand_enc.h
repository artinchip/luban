/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2021 Artinchip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#ifndef _ARTINCHIP_SPINAND_ENC_H_
#define _ARTINCHIP_SPINAND_ENC_H_
#include <crypto/skcipher.h>
#include <linux/aic_spienc.h>

struct spinand_enc_xfer_info {
	u32 addr; /* Cipher data address */
	u32 clen; /* Cipher data len */
};

struct spinand_enc_priv {
	struct spinand_enc_xfer_info xinfo;
	struct crypto_skcipher *tfm;
	struct skcipher_request *req;
	struct completion completion;
	bool encrypt_flag;
	u32 spi_id;
	int rc;
};

static int spinand_enc_init(struct spinand_device *spinand)
{
	struct device *dev = &spinand->spimem->spi->dev;
	struct spinand_enc_priv *priv = NULL;

	if (spinand->priv) {
		dev_err(dev, "spinand->priv is not null.\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct spinand_enc_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "Malloc for priv failed.\n");
		return -ENOMEM;
	}

	priv->encrypt_flag = of_property_read_bool(dev->of_node, "aic,encrypt");
	of_property_read_u32(dev->of_node, "aic,spi-id", &priv->spi_id);
	init_completion(&priv->completion);
	spinand->priv = (void *)priv;

	return 0;
}

static int spinand_enc_get_skcipher(struct spinand_device *spinand)
{
	struct device *dev = &spinand->spimem->spi->dev;
	struct spinand_enc_priv *priv = NULL;
	struct crypto_skcipher *tfm = NULL;
	struct skcipher_request *req = NULL;
	int ret = 0;

	priv = spinand->priv;
	if (!priv) {
		dev_err(dev, "spinand->priv is null.\n");
		return -EINVAL;
	}

	if (!priv->tfm) {
		tfm = crypto_alloc_skcipher("ctr-aes-spienc-aic", 0, 0);
		if (IS_ERR_OR_NULL(tfm)) {
			dev_err(dev, "Alloc skcipher failed.\n");
			ret = -ENODEV;
			tfm = NULL;
			goto err1;
		}
		priv->tfm = tfm;
	}

	if (!priv->req) {
		req = skcipher_request_alloc(tfm, GFP_NOFS);
		if (!req) {
			dev_err(dev, "Alloc skcipher request failed.\n");
			ret = -ENODEV;
			goto err1;
		}

		priv->req = req;
	}

	return 0;
err1:
	if (tfm) {
		crypto_free_skcipher(tfm);
		priv->tfm = NULL;
	}
	if (req) {
		skcipher_request_free(req);
		priv->req = NULL;
	}
	return ret;
}

static void spinand_enc_priv_free(struct spinand_device *spinand)
{
	struct spinand_enc_priv *priv;

	priv = spinand->priv;
	if (!priv)
		return;

	if (priv->tfm)
		crypto_free_skcipher(priv->tfm);
	if (priv->req)
		skcipher_request_free(priv->req);
	if (spinand->priv) {
		kfree(spinand->priv);
		spinand->priv = NULL;
	}
}

static void spinand_enc_xcrypt_done(struct crypto_async_request *req, int rc)
{
	struct spinand_enc_priv *priv = req->data;

	if (rc == -EINPROGRESS)
		return;

	priv->rc = rc;
	complete(&priv->completion);
}

static int spinand_enc_wait(struct spinand_enc_priv *priv)
{
	wait_for_completion(&priv->completion);
	reinit_completion(&priv->completion);

	return priv->rc;
}

static int spinand_enc_xfer_cfg(struct spinand_device *spinand, u32 addr,
				u32 clen)
{
	struct spinand_enc_priv *priv;

	priv = spinand->priv;
	if (!priv)
		return -ENODEV;

	priv->xinfo.addr = addr;
	priv->xinfo.clen = clen;

	return 0;
}

static ssize_t spinand_enc_read(struct spinand_device *spinand,
				struct spi_mem_dirmap_desc *desc, u64 offs,
				size_t len, void *buf)
{
	struct device *dev = &spinand->spimem->spi->dev;
	struct spi_mem_op op = desc->info.op_tmpl;
	struct spinand_enc_priv *priv;
	struct aic_spienc_iv ivinfo;
	bool decrypt = false;
	u32 decrypt_len;
	int ret;

	priv = spinand->priv;
	if (!priv)
		return -ENODEV;

	op.addr.val = desc->info.offset + offs;
	op.data.buf.in = buf;
	op.data.nbytes = len;
	ret = spi_mem_adjust_op_size(desc->mem, &op);
	if (ret)
		return ret;

	decrypt_len = min(priv->xinfo.clen, op.data.nbytes);
	if (decrypt_len && priv->encrypt_flag) {
		/* Need to decrypt the data */
		ret = spinand_enc_get_skcipher(spinand);
		if (!ret)
			decrypt = true;
	}
	if (decrypt) {
		priv->rc = 0;
		memset(&ivinfo, 0, sizeof(ivinfo));
		ivinfo.spi_id = priv->spi_id;
		ivinfo.addr = priv->xinfo.addr;
		ivinfo.cpos = op.cmd.nbytes + op.addr.nbytes + op.dummy.nbytes;

		skcipher_request_set_callback(priv->req, 0,
					      spinand_enc_xcrypt_done, priv);

		skcipher_request_set_crypt(priv->req, NULL, NULL, decrypt_len,
					   &ivinfo);
		ret = crypto_skcipher_decrypt(priv->req);
		if (ret != -EINPROGRESS) {
			dev_err(dev, "SPIEnc Decrypt error, ret = %d.\n", ret);
			return ret;
		}
	}

	ret = spi_mem_exec_op(desc->mem, &op);
	if (ret) {
		dev_err(dev, "spi xfer failed, ret = %d.\n", ret);
		return ret;
	}

	if (decrypt) {
		spinand_enc_wait(spinand->priv);

		/* Update address and remain ciphertext length */
		priv->xinfo.addr += decrypt_len;
		if (priv->xinfo.clen <= decrypt_len)
			priv->xinfo.clen = 0;
		else
			priv->xinfo.clen -= decrypt_len;

		/* Check if all data is 0xFF */
		if (priv->rc == AIC_SPIENC_ALL_FF)
			memset(op.data.buf.in, 0xFF, op.data.nbytes);
	}

	return op.data.nbytes;
}

static ssize_t spinand_enc_write(struct spinand_device *spinand,
				 struct spi_mem_dirmap_desc *desc, u64 offs,
				 size_t len, const void *buf)
{
	struct device *dev = &spinand->spimem->spi->dev;
	struct spi_mem_op op = desc->info.op_tmpl;
	struct spinand_enc_priv *priv;
	struct aic_spienc_iv ivinfo;
	bool encrypt = false;
	u32 encrypt_len;
	int ret;

	priv = spinand->priv;
	if (!priv)
		return -ENODEV;

	op.addr.val = desc->info.offset + offs;
	op.data.buf.out = buf;
	op.data.nbytes = len;
	ret = spi_mem_adjust_op_size(desc->mem, &op);
	if (ret)
		return ret;

	encrypt_len = min(priv->xinfo.clen, op.data.nbytes);
	if (encrypt_len && priv->encrypt_flag) {
		/* Need to encrypt the data */
		ret = spinand_enc_get_skcipher(spinand);
		if (!ret)
			encrypt = true;
	}

	if (encrypt) {
		priv->rc = 0;
		memset(&ivinfo, 0, sizeof(ivinfo));
		ivinfo.spi_id = priv->spi_id;
		ivinfo.addr = priv->xinfo.addr;
		ivinfo.cpos = op.cmd.opcode + op.addr.nbytes + op.dummy.nbytes;

		skcipher_request_set_callback(priv->req, 0,
					      spinand_enc_xcrypt_done, priv);

		skcipher_request_set_crypt(priv->req, NULL, NULL, encrypt_len,
					   &ivinfo);
		ret = crypto_skcipher_encrypt(priv->req);
		if (ret != -EINPROGRESS) {
			dev_err(dev, "SPIEnc encrypt error, ret = %d.\n", ret);
			return ret;
		}
	}

	ret = spi_mem_exec_op(desc->mem, &op);
	if (ret)
		return ret;

	if (encrypt) {
		spinand_enc_wait(spinand->priv);

		/* Update address and remain ciphertext length */
		priv->xinfo.addr += encrypt_len;
		if (priv->xinfo.clen <= encrypt_len)
			priv->xinfo.clen = 0;
		else
			priv->xinfo.clen -= encrypt_len;
	}

	return op.data.nbytes;
}
#endif
