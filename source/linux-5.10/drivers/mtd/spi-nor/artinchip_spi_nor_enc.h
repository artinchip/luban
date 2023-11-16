/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2021 Artinchip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#ifndef _ARTINCHIP_SPI_NOR_ENC_H_
#define _ARTINCHIP_SPI_NOR_ENC_H_
#include <crypto/skcipher.h>
#include <linux/aic_spienc.h>

struct spi_nor_enc_xfer_info {
	u32 addr; /* Cipher data address */
	u32 clen; /* Cipher data len */
};

struct spi_nor_enc_priv {
	struct spi_nor_enc_xfer_info xinfo;
	struct crypto_skcipher *tfm;
	struct skcipher_request *req;
	struct completion completion;
	bool encrypt_flag;
	u32 spi_id;
	int rc;
};

static int spi_nor_enc_init(struct spi_nor *nor)
{
	struct device *dev = &nor->spimem->spi->dev;
	struct spi_nor_enc_priv *priv = NULL;

	if (nor->priv) {
		dev_err(dev, "nor->priv is not null.\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct spi_nor_enc_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "Malloc for priv failed.\n");
		return -ENOMEM;
	}

	priv->encrypt_flag = of_property_read_bool(dev->of_node, "aic,encrypt");
	of_property_read_u32(dev->of_node, "aic,spi-id", &priv->spi_id);
	init_completion(&priv->completion);
	nor->priv = (void *)priv;

	return 0;
}

static int spi_nor_enc_get_skcipher(struct spi_nor *nor)
{
	struct device *dev = &nor->spimem->spi->dev;
	struct spi_nor_enc_priv *priv = NULL;
	struct crypto_skcipher *tfm = NULL;
	struct skcipher_request *req = NULL;
	int ret = 0;

	priv = nor->priv;
	if (!priv) {
		dev_err(dev, "nor->priv is null.\n");
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

static void spi_nor_enc_priv_free(struct spi_nor *nor)
{
	struct spi_nor_enc_priv *priv;

	priv = nor->priv;
	if (!priv)
		return;

	if (priv->tfm)
		crypto_free_skcipher(priv->tfm);
	if (priv->req)
		skcipher_request_free(priv->req);
	if (nor->priv) {
		kfree(nor->priv);
		nor->priv = NULL;
	}
}

static void spi_nor_enc_xcrypt_done(struct crypto_async_request *req, int rc)
{
	struct spi_nor_enc_priv *priv = req->data;

	if (rc == -EINPROGRESS)
		return;

	priv->rc = rc;
	complete(&priv->completion);
}

static int spi_nor_enc_wait(struct spi_nor_enc_priv *priv)
{
	wait_for_completion(&priv->completion);
	reinit_completion(&priv->completion);

	return priv->rc;
}

static int spi_nor_enc_xfer_cfg(struct spi_nor *nor, u32 addr,
				u32 clen)
{
	struct spi_nor_enc_priv *priv;

	priv = nor->priv;
	if (!priv)
		return -ENODEV;

	priv->xinfo.addr = addr;
	priv->xinfo.clen = clen;

	return 0;
}

static ssize_t spi_nor_enc_read(struct spi_nor *nor, loff_t from, size_t len,
				u8 *buf)
{
	struct spi_mem_op op =
		SPI_MEM_OP(SPI_MEM_OP_CMD(nor->read_opcode, 1),
			   SPI_MEM_OP_ADDR(nor->addr_width, from, 1),
			   SPI_MEM_OP_DUMMY(nor->read_dummy, 1),
			   SPI_MEM_OP_DATA_IN(len, buf, 1));
	struct device *dev = &nor->spimem->spi->dev;
	struct spi_nor_enc_priv *priv;
	struct aic_spienc_iv ivinfo;
	bool decrypt = false;
	u32 decrypt_len;
	bool usebouncebuf = false;
	int ret;

	/* get transfer protocols. */
	op.cmd.buswidth = spi_nor_get_protocol_inst_nbits(nor->read_proto);
	op.addr.buswidth = spi_nor_get_protocol_addr_nbits(nor->read_proto);
	op.dummy.buswidth = op.addr.buswidth;
	op.data.buswidth = spi_nor_get_protocol_data_nbits(nor->read_proto);

	/* convert the dummy cycles to the number of bytes */
	op.dummy.nbytes = (nor->read_dummy * op.dummy.buswidth) / 8;

	priv = nor->priv;
	if (!priv)
		return -ENODEV;

	if (object_is_on_stack(buf) || !virt_addr_valid(buf))
		usebouncebuf = true;

	if (usebouncebuf) {
		if (op.data.nbytes > nor->bouncebuf_size)
			op.data.nbytes = nor->bouncebuf_size;
		op.data.buf.in = nor->bouncebuf;
	}

	ret = spi_mem_adjust_op_size(nor->spimem, &op);
	if (ret)
		return ret;

	decrypt_len = min(priv->xinfo.clen, op.data.nbytes);
	if (decrypt_len && priv->encrypt_flag) {
		/* Need to decrypt the data */
		ret = spi_nor_enc_get_skcipher(nor);
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
					      spi_nor_enc_xcrypt_done, priv);

		skcipher_request_set_crypt(priv->req, NULL, NULL, decrypt_len,
					   &ivinfo);
		ret = crypto_skcipher_decrypt(priv->req);
		if (ret != -EINPROGRESS) {
			dev_err(dev, "SPIEnc Decrypt error, ret = %d.\n", ret);
			return ret;
		}
	}

	ret = spi_mem_exec_op(nor->spimem, &op);
	if (ret) {
		dev_err(dev, "spi xfer failed, ret = %d.\n", ret);
		return ret;
	}

	if (decrypt) {
		spi_nor_enc_wait(nor->priv);

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

	if (usebouncebuf)
		memcpy(buf, nor->bouncebuf, op.data.nbytes);

	return op.data.nbytes;
}

static ssize_t spi_nor_enc_write(struct spi_nor *nor, loff_t to, size_t len,
				 const u8 *buf)
{
	struct spi_mem_op op =
		SPI_MEM_OP(SPI_MEM_OP_CMD(nor->program_opcode, 1),
			   SPI_MEM_OP_ADDR(nor->addr_width, to, 1),
			   SPI_MEM_OP_NO_DUMMY,
			   SPI_MEM_OP_DATA_OUT(len, buf, 1));
	struct device *dev = &nor->spimem->spi->dev;
	bool usebouncebuf = false;
	struct spi_nor_enc_priv *priv;
	struct aic_spienc_iv ivinfo;
	bool encrypt = false;
	u32 encrypt_len;
	int ret;

	op.cmd.buswidth = spi_nor_get_protocol_inst_nbits(nor->write_proto);
	op.addr.buswidth = spi_nor_get_protocol_addr_nbits(nor->write_proto);
	op.data.buswidth = spi_nor_get_protocol_data_nbits(nor->write_proto);

	if (nor->program_opcode == SPINOR_OP_AAI_WP && nor->sst_write_second)
		op.addr.nbytes = 0;

	priv = nor->priv;
	if (!priv)
		return -ENODEV;

	if (object_is_on_stack(buf) || !virt_addr_valid(buf))
		usebouncebuf = true;

	if (usebouncebuf) {
		if (op.data.nbytes > nor->bouncebuf_size)
			op.data.nbytes = nor->bouncebuf_size;
		op.data.buf.out = nor->bouncebuf;
		memcpy(nor->bouncebuf, buf, op.data.nbytes);
	}

	ret = spi_mem_adjust_op_size(nor->spimem, &op);
	if (ret)
		return ret;

	encrypt_len = min(priv->xinfo.clen, op.data.nbytes);
	if (encrypt_len && priv->encrypt_flag) {
		/* Need to encrypt the data */
		ret = spi_nor_enc_get_skcipher(nor);
		if (!ret)
			encrypt = true;
	}

	if (encrypt) {
		priv->rc = 0;
		memset(&ivinfo, 0, sizeof(ivinfo));
		ivinfo.spi_id = priv->spi_id;
		ivinfo.addr = priv->xinfo.addr;
		ivinfo.cpos = op.cmd.nbytes + op.addr.nbytes + op.dummy.nbytes;

		skcipher_request_set_callback(priv->req, 0,
					      spi_nor_enc_xcrypt_done, priv);

		skcipher_request_set_crypt(priv->req, NULL, NULL, encrypt_len,
					   &ivinfo);
		ret = crypto_skcipher_encrypt(priv->req);
		if (ret != -EINPROGRESS) {
			dev_err(dev, "SPIEnc encrypt error, ret = %d.\n", ret);
			return ret;
		}
	}

	ret = spi_mem_exec_op(nor->spimem, &op);
	if (ret)
		return ret;

	if (encrypt) {
		spi_nor_enc_wait(nor->priv);

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
