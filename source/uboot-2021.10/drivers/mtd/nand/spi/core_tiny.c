// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016-2017 Micron Technology, Inc.
 *
 * Authors:
 *	Peter Pan <peterpandong@micron.com>
 *	Boris Brezillon <boris.brezillon@bootlin.com>
 *
 * Copyright (c) 2022, ArtInChip Technology Co., Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 *
 */

#define pr_fmt(fmt)	"spi-nand: " fmt

#include <common.h>
#include <errno.h>
#include <watchdog.h>
#include <spi.h>
#include <spi-mem.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/mtd/spinand.h>
#include <artinchip_spinand.h>
#include "manufacturer.h"
#ifdef CONFIG_ARTINCHIP_SPIENC
#include "spi-mem-encrypt.h"
#endif

static struct spinand_device *spi_nand;

static int spinand_noecc_ooblayout_free(struct nand_device *nand, int section,
					struct nand_oob_region *region)
{
	if (section)
		return -ERANGE;

	/* Reserve 2 bytes for the BBM. */
	region->offset = 2;
	region->length = 62;

	return 0;
}

static int spinand_ooblayout_find_region(struct nand_device *nand, int byte,
			int *sectionp, struct nand_oob_region *oobregion,
			int (*iter)(struct nand_device *,
				    int section,
				    struct nand_oob_region *oobregion))
{
	int pos = 0, ret, section = 0;

	memset(oobregion, 0, sizeof(*oobregion));

	while (1) {
		ret = iter(nand, section, oobregion);
		if (ret)
			return ret;

		if (pos + oobregion->length > byte)
			break;

		pos += oobregion->length;
		section++;
	}

	/*
	 * Adjust region info to make it start at the beginning at the
	 * 'start' ECC byte.
	 */
	oobregion->offset += byte - pos;
	oobregion->length -= byte - pos;
	*sectionp = section;

	return 0;
}

static int spinand_ooblayout_get_bytes(struct nand_device *nand, u8 *buf,
				const u8 *oobbuf, int start, int nbytes,
				int (*iter)(struct nand_device *,
					    int section,
					    struct nand_oob_region *oobregion))
{
	struct nand_oob_region oobregion;
	int section, ret;

	ret = spinand_ooblayout_find_region(nand, start, &section,
					&oobregion, iter);

	while (!ret) {
		int cnt;

		cnt = min_t(int, nbytes, oobregion.length);
		memcpy(buf, oobbuf + oobregion.offset, cnt);
		buf += cnt;
		nbytes -= cnt;

		if (!nbytes)
			break;

		ret = iter(nand, ++section, &oobregion);
	}

	return ret;
}

static void spinand_cache_op_adjust_colum(struct spinand_device *spinand,
					  const struct nand_page_io_req *req,
					  u16 *column)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	unsigned int shift;

	if (nand->memorg.planes_per_lun < 2)
		return;

	/* The plane number is passed in MSB just above the column address */
	shift = fls(nand->memorg.pagesize);
	*column |= req->pos.plane << shift;
}

static int spinand_read_reg_op(struct spinand_device *spinand, u8 reg, u8 *val)
{
	struct spi_mem_op op = SPINAND_GET_FEATURE_OP(reg,
						      spinand->scratchbuf);
	int ret;

	ret = spi_mem_exec_op(spinand->slave, &op);
	if (ret)
		return ret;

	*val = *spinand->scratchbuf;
	return 0;
}

static int spinand_write_reg_op(struct spinand_device *spinand, u8 reg, u8 val)
{
	struct spi_mem_op op = SPINAND_SET_FEATURE_OP(reg,
						      spinand->scratchbuf);

	*spinand->scratchbuf = val;
	return spi_mem_exec_op(spinand->slave, &op);
}

static int spinand_read_status(struct spinand_device *spinand, u8 *status)
{
	return spinand_read_reg_op(spinand, REG_STATUS, status);
}

static int spinand_get_cfg(struct spinand_device *spinand, u8 *cfg)
{
	struct nand_device *nand = spinand_to_nand(spinand);

	if (WARN_ON(spinand->cur_target < 0 ||
		    spinand->cur_target >= nand->memorg.ntargets))
		return -EINVAL;

	*cfg = spinand->cfg_cache[spinand->cur_target];
	return 0;
}

static int spinand_set_cfg(struct spinand_device *spinand, u8 cfg)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	int ret;

	if (WARN_ON(spinand->cur_target < 0 ||
		    spinand->cur_target >= nand->memorg.ntargets))
		return -EINVAL;

	if (spinand->cfg_cache[spinand->cur_target] == cfg)
		return 0;

	ret = spinand_write_reg_op(spinand, REG_CFG, cfg);
	if (ret)
		return ret;

	spinand->cfg_cache[spinand->cur_target] = cfg;
	return 0;
}

/**
 * spinand_upd_cfg() - Update the configuration register
 * @spinand: the spinand device
 * @mask: the mask encoding the bits to update in the config reg
 * @val: the new value to apply
 *
 * Update the configuration register.
 *
 * Return: 0 on success, a negative error code otherwise.
 */
int spinand_upd_cfg(struct spinand_device *spinand, u8 mask, u8 val)
{
	int ret;
	u8 cfg;

	ret = spinand_get_cfg(spinand, &cfg);
	if (ret)
		return ret;

	cfg &= ~mask;
	cfg |= val;

	return spinand_set_cfg(spinand, cfg);
}

/**
 * spinand_select_target() - Select a specific NAND target/die
 * @spinand: the spinand device
 * @target: the target/die to select
 *
 * Select a new target/die. If chip only has one die, this function is a NOOP.
 *
 * Return: 0 on success, a negative error code otherwise.
 */
int spinand_select_target(struct spinand_device *spinand, unsigned int target)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	int ret;

	if (WARN_ON(target >= nand->memorg.ntargets))
		return -EINVAL;

	if (spinand->cur_target == target)
		return 0;

	if (nand->memorg.ntargets == 1) {
		spinand->cur_target = target;
		return 0;
	}

	ret = spinand->select_target(spinand, target);
	if (ret)
		return ret;

	spinand->cur_target = target;
	return 0;
}

static int spinand_init_cfg_cache(struct spinand_device *spinand)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	struct udevice *dev = spinand->slave->dev;
	unsigned int target;
	int ret;

	spinand->cfg_cache = devm_kzalloc(dev,
					  sizeof(*spinand->cfg_cache) *
					  nand->memorg.ntargets,
					  GFP_KERNEL);
	if (!spinand->cfg_cache)
		return -ENOMEM;

	for (target = 0; target < nand->memorg.ntargets; target++) {
		ret = spinand_select_target(spinand, target);
		if (ret)
			return ret;

		/*
		 * We use spinand_read_reg_op() instead of spinand_get_cfg()
		 * here to bypass the config cache.
		 */
		ret = spinand_read_reg_op(spinand, REG_CFG,
					  &spinand->cfg_cache[target]);
		if (ret)
			return ret;
	}

	return 0;
}

static int spinand_init_quad_enable(struct spinand_device *spinand)
{
	bool enable = false;

	if (!(spinand->flags & SPINAND_HAS_QE_BIT))
		return 0;

	if (spinand->op_templates.read_cache->data.buswidth == 4 ||
	    spinand->op_templates.write_cache->data.buswidth == 4 ||
	    spinand->op_templates.update_cache->data.buswidth == 4)
		enable = true;

	return spinand_upd_cfg(spinand, CFG_QUAD_ENABLE,
			       enable ? CFG_QUAD_ENABLE : 0);
}

#ifdef CONFIG_SPI_NAND_WINBOND_CONT_READ
static void spinand_init_continuous_read(struct spinand_device *spinand)
{
	spinand->use_continuous_read = false;
}

static int spinand_continuous_read_enable(struct spinand_device *spinand,
					  bool enable)
{
	int ret = spinand_upd_cfg(spinand, CFG_BUF_ENABLE,
		       enable ? 0 : CFG_BUF_ENABLE);
	if (!ret)
		spinand->use_continuous_read = enable;
	return ret;
}
#endif

static int spinand_ecc_enable(struct spinand_device *spinand,
			      bool enable)
{
	return spinand_upd_cfg(spinand, CFG_ECC_ENABLE,
			       enable ? CFG_ECC_ENABLE : 0);
}

static int spinand_load_page_op(struct spinand_device *spinand,
				const struct nand_page_io_req *req)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	unsigned int row = nanddev_pos_to_row(nand, &req->pos);
	struct spi_mem_op op = SPINAND_PAGE_READ_OP(row);

	return spi_mem_exec_op(spinand->slave, &op);
}

static int spinand_read_from_cache_op(struct spinand_device *spinand,
				      const struct nand_page_io_req *req)
{
	struct spi_mem_op op = *spinand->op_templates.read_cache;
#ifdef CONFIG_SPI_NAND_WINBOND_CONT_READ
	struct spi_mem_op op_cont = SPINAND_PAGE_READ_FROM_CACHE_X4_OP_CONT(4,
	NULL, 0);

	if (spinand->use_continuous_read)
		memcpy(&op, &op_cont, sizeof(struct spi_mem_op));
#endif
	struct nand_device *nand = spinand_to_nand(spinand);
	struct nand_page_io_req adjreq = *req;
	unsigned int nbytes = 0;
	void *buf = NULL;
	u16 column = 0;
	int ret, nocopyflag = 0;

	if (spinand->use_continuous_read) {
		buf = req->databuf.in;
		nbytes = req->datalen;
	} else if (req->datalen) {
		adjreq.datalen = nanddev_page_size(nand);
		adjreq.dataoffs = 0;
		adjreq.databuf.in = spinand->databuf;
		buf = spinand->databuf;
		nbytes = adjreq.datalen;
	}

	if (req->ooblen) {
		adjreq.ooblen = nanddev_per_page_oobsize(nand);
		adjreq.ooboffs = 0;
		adjreq.oobbuf.in = spinand->oobbuf;
		nbytes += nanddev_per_page_oobsize(nand);
		if (!buf) {
			buf = spinand->oobbuf;
			column = nanddev_page_size(nand);
		}
	}
	if (req->datalen > 0 && req->ooblen == 0 && req->databuf.in) {
		nocopyflag = 1;
		adjreq.databuf.in = req->databuf.in;
		buf = req->databuf.in;
		nbytes = adjreq.datalen;
	}

	spinand_cache_op_adjust_colum(spinand, &adjreq, &column);
	op.addr.val = column;

#ifdef CONFIG_ARTINCHIP_SPIENC
	spi_mem_enc_xfer_cfg(spinand->priv,
			     nanddev_pos_to_offs(nand, &req->pos),
			     adjreq.datalen, req->mode);
#endif
	/*
	 * Some controllers are limited in term of max RX data size. In this
	 * case, just repeat the READ_CACHE operation after updating the
	 * column.
	 */
	while (nbytes) {
		op.data.buf.in = buf;
		op.data.nbytes = nbytes;
		ret = spi_mem_adjust_op_size(spinand->slave, &op);
		if (ret)
			return ret;

#ifdef CONFIG_ARTINCHIP_SPIENC
		ret = spi_mem_enc_read(spinand->priv, &op);
#else
		ret = spi_mem_exec_op(spinand->slave, &op);
#endif
		if (ret)
			return ret;

		buf += op.data.nbytes;
		nbytes -= op.data.nbytes;
		op.addr.val += op.data.nbytes;
	}

	if (nocopyflag || spinand->use_continuous_read)
		return 0;

	if (req->datalen)
		memcpy(req->databuf.in, spinand->databuf + req->dataoffs,
		       req->datalen);

	if (req->ooblen) {
		if (req->mode == MTD_OPS_AUTO_OOB)
			spinand_ooblayout_get_bytes(nand,
						    req->oobbuf.in,
						    spinand->oobbuf,
						    req->ooboffs,
						    req->ooblen,
						    spinand_noecc_ooblayout_free);
		else
			memcpy(req->oobbuf.in, spinand->oobbuf + req->ooboffs,
			       req->ooblen);
	}

	return 0;
}

static int spinand_wait(struct spinand_device *spinand, u8 *s)
{
	unsigned long start, stop;
	u8 status;
	int ret;

	start = get_timer(0);
	stop = 400;
	do {
		ret = spinand_read_status(spinand, &status);
		if (ret)
			return ret;

		if (!(status & STATUS_BUSY))
			goto out;
	} while (get_timer(start) < stop);

	/*
	 * Extra read, just in case the STATUS_READY bit has changed
	 * since our last check
	 */
	ret = spinand_read_status(spinand, &status);
	if (ret)
		return ret;

out:
	if (s)
		*s = status;

	return status & STATUS_BUSY ? -ETIMEDOUT : 0;
}

static int spinand_read_id_op(struct spinand_device *spinand, u8 *buf)
{
	struct spi_mem_op op = SPINAND_READID_OP(0, spinand->scratchbuf,
						 SPINAND_MAX_ID_LEN);
	int ret;

	ret = spi_mem_exec_op(spinand->slave, &op);
	if (!ret)
		memcpy(buf, spinand->scratchbuf, SPINAND_MAX_ID_LEN);

	return ret;
}

static int spinand_reset_op(struct spinand_device *spinand)
{
	struct spi_mem_op op = SPINAND_RESET_OP;
	int ret;

	ret = spi_mem_exec_op(spinand->slave, &op);
	if (ret)
		return ret;

	return spinand_wait(spinand, NULL);
}

static int spinand_lock_block(struct spinand_device *spinand, u8 lock)
{
	return spinand_write_reg_op(spinand, REG_BLOCK_LOCK, lock);
}

static int spinand_check_ecc_status(struct spinand_device *spinand, u8 status)
{
	struct nand_device *nand = spinand_to_nand(spinand);

	if (spinand->eccinfo.get_status)
		return spinand->eccinfo.get_status(spinand, status);

	switch (status & STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case STATUS_ECC_HAS_BITFLIPS:
		/*
		 * We have no way to know exactly how many bitflips have been
		 * fixed, so let's return the maximum possible value so that
		 * wear-leveling layers move the data immediately.
		 */
		return nand->eccreq.strength;

	case STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	default:
		break;
	}

	return -EINVAL;
}

static int spinand_read_page(struct spinand_device *spinand,
			     const struct nand_page_io_req *req,
			     bool ecc_enabled)
{
	u8 status;
	int ret;

	ret = spinand_load_page_op(spinand, req);
	if (ret)
		return ret;

	ret = spinand_wait(spinand, &status);
	if (ret < 0)
		return ret;

	ret = spinand_read_from_cache_op(spinand, req);
	if (ret)
		return ret;

	if (!ecc_enabled)
		return 0;

	return spinand_check_ecc_status(spinand, status);
}

#ifdef CONFIG_SPI_NAND_WINBOND_CONT_READ
int spinand_continuous_read(struct spinand_device *spinand, loff_t from,
			    size_t len, size_t *retlen, u_char *buf,
			    struct nand_io_iter *iter)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	int ret = 0;

	bool enable_cont_read = true;
	bool enable_ecc = false;

	iter->req.mode = MTD_OPS_RAW;
	iter->req.ooblen = 0;

	*retlen = 0;

	/* Read the first unaligned page with conventional read */
	if (from & (nanddev_page_size(nand) - 1)) {
		iter->req.databuf.in = buf;
		iter->req.dataoffs = nanddev_offs_to_pos(nand, from,
							 &iter->req.pos);

		iter->req.datalen = nanddev_page_size(nand) -
			iter->req.dataoffs;

		ret = spinand_select_target(spinand, iter->req.pos.target);
		if (ret)
			return ret;

		ret = spinand_read_page(spinand, &iter->req, enable_ecc);
		if (ret)
			return ret;
		*retlen += iter->req.datalen;
	}

	iter->req.dataoffs = nanddev_offs_to_pos(nand, from + *retlen,
						 &iter->req.pos);
	iter->req.databuf.in = buf + *retlen;
	iter->req.datalen = len - *retlen;

	ret = spinand_continuous_read_enable(spinand, enable_cont_read);
	if (ret)
		return ret;

	ret = spinand_select_target(spinand, iter->req.pos.target);
	if (ret) {
		*retlen = 0;
		goto continuous_read_error;
	}

	/* use continuous mode to read all the remaining data at once */
	ret = spinand_read_page(spinand, &iter->req, enable_ecc);
	if (ret) {
		*retlen = 0;
		goto continuous_read_error;
	}

	ret = spinand_reset_op(spinand);
	if (ret) {
		*retlen = 0;
		goto continuous_read_error;
	}

	*retlen += iter->req.datalen;

continuous_read_error:
	enable_cont_read = false;
	ret = spinand_continuous_read_enable(spinand, enable_cont_read);

	return ret;
}
#endif

int spinand_read(struct spinand_device *spinand, loff_t from,
		 size_t len, size_t *retlen, u_char *buf)
{
	int ret_code;
	*retlen = 0;
	struct nand_oob_ops ops_temp = {
		.len = len,
		.datbuf = buf,
	};
	struct nand_oob_ops *ops = &ops_temp;
	struct nand_device *nand = spinand_to_nand(spinand);
	unsigned int max_bitflips = 0;
	struct nand_io_iter iter;
	bool enable_ecc = false;
	bool ecc_failed = false;
	int ret = 0;

	if (from < 0 || from > nand->info->size || len > nand->info->size - from)
		return -EINVAL;

	if (!len)
		return 0;

	if (ops->mode != MTD_OPS_RAW && spinand->eccinfo.ooblayout)
		enable_ecc = true;

#ifdef CONFIG_SPI_NAND_WINBOND_CONT_READ
/**
 * if the device supports continuous read mode and the read length is
 * greater then one page size,the device will enter the continuous read mode
 * this mode helps avoiding issuing a page read command and read from cache
 * command again,and improves the performance of reading continuous pages.
 */
	if ((spinand->flags & SPINAND_HAS_CONT_READ_BIT) && len >
	     nanddev_page_size(nand)) {
		ret = spinand_continuous_read(spinand, from, len, &ops->retlen,
					      buf, &iter);
		goto continuous_read_finish;
	}
#endif

	nanddev_io_for_each_page(nand, from, ops, &iter) {
		WATCHDOG_RESET();
		ret = spinand_select_target(spinand, iter.req.pos.target);
		if (ret)
			break;

		ret = spinand_ecc_enable(spinand, enable_ecc);
		if (ret)
			break;

		ret = spinand_read_page(spinand, &iter.req, enable_ecc);
		if (ret < 0 && ret != -EBADMSG)
			break;

		if (ret == -EBADMSG) {
			ecc_failed = true;
			nand->info->ecc_stats.failed++;
			ret = 0;
		} else {
			nand->info->ecc_stats.corrected += ret;
			max_bitflips = max_t(unsigned int, max_bitflips, ret);
		}

		ops->retlen += iter.req.datalen;
		ops->oobretlen += iter.req.ooblen;
	}

#ifdef CONFIG_SPI_NAND_WINBOND_CONT_READ
continuous_read_finish:
#endif

	if (ecc_failed && !ret)
		ret = -EBADMSG;

	ret_code = ret ? ret : max_bitflips;
	*retlen = ops->retlen;

	if (unlikely(ret_code < 0))
		return ret_code;

	if (nand->info->ecc_strength == 0)
		return 0;       /* device lacks ecc */

	return ret_code >= nand->info->bitflip_threshold ? -EUCLEAN : 0;
}

static bool spinand_isbad(struct nand_device *nand, const struct nand_pos *pos)
{
	struct spinand_device *spinand = nand_to_spinand(nand);
	u8 marker[2] = { };
	struct nand_page_io_req req = {
		.pos = *pos,
		.ooblen = sizeof(marker),
		.ooboffs = 0,
		.oobbuf.in = marker,
		.mode = MTD_OPS_RAW,
	};
	int ret;

	ret = spinand_select_target(spinand, pos->target);
	if (ret)
		return ret;

	ret = spinand_read_page(spinand, &req, false);
	if (ret)
		return ret;

	if (marker[0] != 0xff || marker[1] != 0xff)
		return true;

	return false;
}

int spinand_block_isbad(struct spinand_device *spinand, loff_t offs)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	struct nand_pos pos;
	int ret;

	if (offs < 0 || offs > nanddev_size(nand))
		return -EINVAL;

	nanddev_offs_to_pos(nand, offs, &pos);
	ret = nanddev_isbad(nand, &pos);
	return ret;
}

const struct spi_mem_op *
spinand_find_supported_op(struct spinand_device *spinand,
			  const struct spi_mem_op *ops,
			  unsigned int nops)
{
	unsigned int i;

	for (i = 0; i < nops; i++) {
		if (spi_mem_supports_op(spinand->slave, &ops[i]))
			return &ops[i];
	}

	return NULL;
}

static const struct nand_ops spinand_ops = {
	.isbad = spinand_isbad,
};

static const struct spi_mem_op *
spinand_select_op_variant(struct spinand_device *spinand,
			  const struct spinand_op_variants *variants)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	unsigned int i;

	for (i = 0; i < variants->nops; i++) {
		struct spi_mem_op op = variants->ops[i];
		unsigned int nbytes;
		int ret;

		nbytes = nanddev_per_page_oobsize(nand) +
			 nanddev_page_size(nand);

		while (nbytes) {
			op.data.nbytes = nbytes;
			ret = spi_mem_adjust_op_size(spinand->slave, &op);
			if (ret)
				break;

			if (!spi_mem_supports_op(spinand->slave, &op))
				break;

			nbytes -= op.data.nbytes;
		}

		if (!nbytes)
			return &variants->ops[i];
	}

	return NULL;
}

/**
 * spinand_match_and_init() - Try to find a match between a device ID and an
 *			      entry in a spinand_info table
 * @spinand: SPI NAND object
 * @table: SPI NAND device description table
 * @table_size: size of the device description table
 *
 * Should be used by SPI NAND manufacturer drivers when they want to find a
 * match between a device ID retrieved through the READ_ID command and an
 * entry in the SPI NAND description table. If a match is found, the spinand
 * object will be initialized with information provided by the matching
 * spinand_info entry.
 *
 * Return: 0 on success, a negative error code otherwise.
 */
int spinand_match_and_init(struct spinand_device *spinand,
			   const struct spinand_info *table,
			   unsigned int table_size, u8 *devid)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	unsigned int i;

	for (i = 0; i < table_size; i++) {
		const struct spinand_info *info = &table[i];
		const struct spi_mem_op *op;

		if (memcmp(devid, info->devid.id, info->devid.len))
			continue;

		nand->memorg = table[i].memorg;
		nand->eccreq = table[i].eccreq;
		spinand->eccinfo = table[i].eccinfo;
		spinand->flags = table[i].flags;
		spinand->select_target = table[i].select_target;

		op = spinand_select_op_variant(spinand,
					       info->op_variants.read_cache);
		if (!op)
			return -ENOTSUPP;

		spinand->op_templates.read_cache = op;

		op = spinand_select_op_variant(spinand,
					       info->op_variants.write_cache);
		if (!op)
			return -ENOTSUPP;

		spinand->op_templates.write_cache = op;

		op = spinand_select_op_variant(spinand,
					       info->op_variants.update_cache);
		spinand->op_templates.update_cache = op;

		return 0;
	}

	return -ENOTSUPP;
}

static int spinand_detect(struct spinand_device *spinand)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	int ret;

	ret = spinand_reset_op(spinand);
	if (ret)
		return ret;

	ret = spinand_read_id_op(spinand, spinand->id.data);
	if (ret)
		return ret;

	spinand->id.len = SPINAND_MAX_ID_LEN;

	ret = spinand_manufacturer_detect(spinand);
	if (ret) {
		dev_err(spinand->slave->dev,
			"unknown raw ID %02x%02x%02x%02x\n",
			spinand->id.data[0], spinand->id.data[1],
			spinand->id.data[2], spinand->id.data[3]);
		return ret;
	}

	if (nand->memorg.ntargets > 1 && !spinand->select_target) {
		dev_err(spinand->slave->dev,
			"SPI NANDs with more than one die must implement ->select_target()\n");
		return -EINVAL;
	}

	dev_info(spinand->slave->dev, "RAW ID %02x%02x%02x%02x\n",
		 spinand->id.data[0], spinand->id.data[1], spinand->id.data[2],
		 spinand->id.data[3]);
	dev_info(spinand->slave->dev,
		 "%s SPI NAND was found.\n", spinand->manufacturer->name);
	dev_info(spinand->slave->dev,
		 "%llu MiB, block size: %zu KiB, page size: %zu, OOB size: %u\n",
		 nanddev_size(nand) >> 20, nanddev_eraseblock_size(nand) >> 10,
		 nanddev_page_size(nand), nanddev_per_page_oobsize(nand));

	return 0;
}

static int spinand_init(struct spinand_device *spinand)
{
	struct nand_device *nand = spinand_to_nand(spinand);
	int ret, i;

	/*
	 * We need a scratch buffer because the spi_mem interface requires that
	 * buf passed in spi_mem_op->data.buf be DMA-able.
	 */
	spinand->scratchbuf = kzalloc(SPINAND_MAX_ID_LEN, GFP_KERNEL);
	if (!spinand->scratchbuf)
		return -ENOMEM;

	ret = spinand_detect(spinand);
	if (ret)
		goto err_free_bufs;

	/*
	 * Use kzalloc() instead of devm_kzalloc() here, because some drivers
	 * may use this buffer for DMA access.
	 * Memory allocated by devm_ does not guarantee DMA-safe alignment.
	 */
	spinand->databuf = kzalloc(nanddev_page_size(nand) +
			       nanddev_per_page_oobsize(nand),
			       GFP_KERNEL);
	if (!spinand->databuf) {
		ret = -ENOMEM;
		goto err_free_bufs;
	}

	spinand->oobbuf = spinand->databuf + nanddev_page_size(nand);

	ret = spinand_init_cfg_cache(spinand);
	if (ret)
		goto err_free_bufs;

	ret = spinand_init_quad_enable(spinand);
	if (ret)
		goto err_free_bufs;

	ret = spinand_upd_cfg(spinand, CFG_OTP_ENABLE, 0);
	if (ret)
		goto err_free_bufs;

	ret = spinand_manufacturer_init(spinand);
	if (ret) {
		dev_err(spinand->slave->dev,
			"Failed to initialize the SPI NAND chip (err = %d)\n",
			ret);
		goto err_free_bufs;
	}

	/* After power up, all blocks are locked, so unlock them here. */
	for (i = 0; i < nand->memorg.ntargets; i++) {
		ret = spinand_select_target(spinand, i);
		if (ret)
			goto err_free_bufs;

		ret = spinand_lock_block(spinand, BL_ALL_UNLOCKED);
		if (ret)
			goto err_free_bufs;
	}

	ret = nanddev_init(nand, &spinand_ops, THIS_MODULE);
	if (ret)
		goto err_manuf_cleanup;

#ifdef CONFIG_SPI_NAND_WINBOND_CONT_READ
	/* Init buf read mode */
	spinand_init_continuous_read(spinand);
#endif

	return 0;

err_manuf_cleanup:
	spinand_manufacturer_cleanup(spinand);

err_free_bufs:
	kfree(spinand->databuf);
	kfree(spinand->scratchbuf);
	return ret;
}

static int spinand_probe(struct udevice *dev)
{
	struct spinand_device *spinand = dev_get_priv(dev);
	struct spi_slave *slave = dev_get_parent_priv(dev);
	int ret;

	spinand->slave = slave;

	ret = spinand_init(spinand);
	if (ret)
		return ret;

#ifdef CONFIG_ARTINCHIP_SPIENC
	spinand->priv = spi_mem_enc_init(spinand->slave);
#endif
	spi_nand = spinand;

	return 0;
}

struct spinand_device *get_spinand(void)
{
	return spi_nand;
}

static const struct udevice_id spinand_ids[] = {
	{ .compatible = "spi-nand" },
	{ /* sentinel */ },
};

U_BOOT_DRIVER(spinand) = {
	.name = "spi_nand",
	.id = UCLASS_MTD,
	.of_match = spinand_ids,
	.priv_auto	= sizeof(struct spinand_device),
	.probe = spinand_probe,
};
