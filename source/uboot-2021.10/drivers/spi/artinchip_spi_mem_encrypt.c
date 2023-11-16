// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */
#include <common.h>
#include <dm.h>
#include <misc.h>
#include <artinchip/aic_spienc.h>
#include <spi.h>
#include <spi-mem.h>
#include <spi-mem-encrypt.h>
#include <mtd/mtd-abi.h>

struct spi_mem_enc_xfer_info {
	u32 addr;
	u32 clen;
	int mode;
};

struct spi_mem_enc_priv {
	struct spi_slave *slave;
	struct udevice *spienc_dev;
	struct spi_mem_enc_xfer_info xinfo;
	bool encrypt_flag;
	u32 spi_id;
} spi_mem_enc;

void *spi_mem_enc_init(struct spi_slave *ss)
{
	struct spi_mem_enc_priv *p;
	struct udevice *dev = NULL;
	int ret = 0;

	p = &spi_mem_enc;
	ret = uclass_first_device_err(UCLASS_MISC, &dev);
	if (ret) {
		pr_err("Get UCLASS_MISC device failed.\n");
		return NULL;
	}

	do {
		if (device_is_compatible(dev, "artinchip,aic-spienc-v1.0"))
			break;
		else
			dev = NULL;
		ret = uclass_next_device_err(&dev);
	} while (dev);

	memset(p, 0, sizeof(*p));
	p->spienc_dev = dev;
	p->slave = ss;
	p->encrypt_flag = dev_read_bool(ss->dev, "aic,encrypt");
	p->spi_id = dev_read_u32_default(ss->dev, "aic,spi-id", 0xFF);

	return p;
}

int spi_mem_enc_xfer_cfg(void *handle, u32 addr, u32 clen, int mode)
{
	struct spi_mem_enc_priv *p;

	if (!handle)
		return -EINVAL;

	p = handle;
	p->xinfo.addr = addr;
	p->xinfo.clen = clen;
	p->xinfo.mode = mode;

	return 0;
}

static int spi_mem_enc_start(struct spi_mem_enc_priv *priv,
			     struct spienc_crypt_cfg *cfg)
{
	int ret = 0;

	ret = misc_ioctl(priv->spienc_dev, AIC_SPIENC_IOCTL_CRYPT_CFG, cfg);
	if (ret)
		return ret;

	ret = misc_ioctl(priv->spienc_dev, AIC_SPIENC_IOCTL_START, NULL);
	return ret;
}

static int spi_mem_enc_stop(struct spi_mem_enc_priv *priv)
{
	int ret = 0;

	ret = misc_ioctl(priv->spienc_dev, AIC_SPIENC_IOCTL_STOP, NULL);

	return ret;
}

static bool spi_mem_enc_check_empty(struct spi_mem_enc_priv *priv)
{
	u32 sts;
	int ret = 0;

	ret = misc_ioctl(priv->spienc_dev, AIC_SPIENC_IOCTL_CHECK_EMPTY, &sts);
	if (ret)
		return false;

	return (bool)sts;
}

int spi_mem_enc_read(void *handle, struct spi_mem_op *op)
{
	struct spi_mem_enc_priv *p;
	struct spienc_crypt_cfg cfg;
	bool decrypt, empty;
	u32 clen, cpos;
	int ret;

	if (!handle)
		return -EINVAL;
	p = handle;

	clen = min(p->xinfo.clen, op->data.nbytes);
	decrypt = (p->xinfo.mode != MTD_OPS_NO_ENC);
	if (clen == 0 || !p->spienc_dev || !p->encrypt_flag)
		decrypt = false;

	if (decrypt) {
		memset(&cfg, 0, sizeof(cfg));
		cpos = op->cmd.nbytes + op->addr.nbytes + op->dummy.nbytes;
		cfg.spi_id = p->spi_id;
		cfg.addr = p->xinfo.addr;
		cfg.cpos = cpos;
		cfg.clen = clen;

		ret = spi_mem_enc_start(p, &cfg);
		if (ret) {
			pr_err("Start SPIEnc error, ret = %d\n", ret);
			return ret;
		}
	}

	ret = spi_mem_exec_op(p->slave, op);

	if (decrypt) {
		spi_mem_enc_stop(p);
		empty = spi_mem_enc_check_empty(p);
		if (empty)
			memset(op->data.buf.in, 0xFF, op->data.nbytes);

		p->xinfo.addr += clen;
		if (p->xinfo.clen <= clen)
			p->xinfo.clen = 0;
		else
			p->xinfo.clen -= clen;
	}

	return ret;
}

int spi_mem_enc_write(void *handle, struct spi_mem_op *op)
{
	struct spi_mem_enc_priv *p;
	struct spienc_crypt_cfg cfg;
	u32 clen, cpos;
	bool encrypt;
	int ret;

	if (!handle)
		return -EINVAL;
	p = handle;

	clen = min(p->xinfo.clen, op->data.nbytes);
	encrypt = (p->xinfo.mode != MTD_OPS_NO_ENC);
	if (clen == 0 || !p->spienc_dev || !p->encrypt_flag)
		encrypt = false;

	if (encrypt) {
		memset(&cfg, 0, sizeof(cfg));
		cpos = op->cmd.nbytes + op->addr.nbytes + op->dummy.nbytes;
		cfg.spi_id = p->spi_id;
		cfg.addr = p->xinfo.addr;
		cfg.cpos = cpos;
		cfg.clen = clen;

		ret = spi_mem_enc_start(p, &cfg);
		if (ret) {
			pr_err("Start SPIEnc error, ret = %d\n", ret);
			return ret;
		}
	}

	ret = spi_mem_exec_op(p->slave, op);

	if (encrypt) {
		spi_mem_enc_stop(p);

		p->xinfo.addr += clen;
		if (p->xinfo.clen <= clen)
			p->xinfo.clen = 0;
		else
			p->xinfo.clen -= clen;
	}

	return ret;
}
