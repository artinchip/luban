// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */
#include <common.h>
#include <linux/math64.h>
#include <image-sparse.h>
#include <sparse_format.h>
#include <artinchip/aicupg.h>
#include "upg_internal.h"
#include <mmc.h>
#include <env.h>

#if 0
#undef debug
#define debug printf
#endif

#define MMC_BLOCK_SIZE   512
#define SPARSE_FILLBUF_SIZE (1024 * 1024)

struct aicupg_mmc_priv {
	struct mmc *mmc;
	struct disk_partition part_info;
	sparse_header_t sparse_header;
	chunk_header_t chunk_header;
	unsigned char remain_data[MMC_BLOCK_SIZE];
	unsigned int remain_len;
	lbaint_t blkstart;
	int cur_chunk;
	int cur_chunk_remain_data_sz;
	int cur_chunk_burned_data_sz;
	int is_sparse;
};

static struct mmc *init_mmc(int dev)
{
	struct mmc *mmc;

	mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("no mmc device at slot %x\n", dev);
		return NULL;
	}

	if (mmc_init(mmc))
		return NULL;

#ifdef CONFIG_SUPPORT_EMMC_BOOT
	if (!IS_SD(mmc)) {
		/*
		 * For eMMC:
		 * Set RST_n_FUNCTION to 1: enable it
		 */
		if (mmc->ext_csd[EXT_CSD_RST_N_FUNCTION] == 0)
			mmc_set_rst_n_function(mmc, 1);
	}
#endif

#ifdef CONFIG_BLOCK_CACHE
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	blkcache_invalidate(bd->if_type, bd->devnum);
#endif
	return mmc;
}

s32 mmc_fwc_prepare(struct fwc_info *fwc, u32 mmc_id)
{
	int ret = 0;

	/*Set GPT partition*/
	ret = aicupg_mmc_create_gpt_part(mmc_id, false);
	if (ret < 0) {
		pr_err("Create GPT partitions failed\n");
		return ret;
	}
	return ret;
}

s32 mmc_is_exist(struct fwc_info *fwc, u32 mmc_id)
{
	if (mmc_id >= get_mmc_num()) {
		pr_err("Invalid mmc dev %d\n", mmc_id);
		return -ENODEV;
	}

	return 0;
}

void mmc_fwc_start(struct fwc_info *fwc)
{
	struct aicupg_mmc_priv *priv;
	struct blk_desc *desc;
	int dev, ret = 0;

	pr_debug("%s, FWC name %s\n", __func__, fwc->meta.name);
	fwc->block_size = MMC_BLOCK_SIZE;
	fwc->burn_result = 0;
	fwc->run_result = 0;
	priv = malloc(sizeof(struct aicupg_mmc_priv));
	if (!priv)
		goto err;
	memset(priv, 0, sizeof(struct aicupg_mmc_priv));

	dev = get_current_device_id();
	priv->mmc = init_mmc(dev);
	if (!priv->mmc) {
		pr_err("init mmc failed.\n");
		goto err;
	}
	desc = mmc_get_blk_desc(priv->mmc);
	ret = part_get_info_by_name(desc, fwc->meta.partition,
				    &priv->part_info);
	if (ret == -1) {
		pr_err("Get partition information failed.\n");
		goto err;
	}
	fwc->priv = priv;
	return;
err:
	if (priv)
		free(priv);
}

s32 mmc_fwc_sparse_write(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_mmc_priv *priv;
	struct disk_partition *part_info;
	sparse_header_t *sheader;
	chunk_header_t *cheader;
	struct blk_desc *desc;
	u8 *wbuf, *p;
	s32 clen = 0, remain, total_len;
	u32 chunk;
	u64 chunk_data_sz, chunk_blkcnt, remain_blkcnt;
	u32 total_blocks = 0, blks;
	u32 remain_blks, redund_blks, erase_group;
	u32 *fill_buf, fill_val, fill_buf_num_blks;
	int i, j, break_flag = 0;

	wbuf = malloc(ROUNDUP(len + MMC_BLOCK_SIZE, fwc->block_size));
	if (!wbuf) {
		pr_err("malloc failed.\n");
		return 0;
	}
	p = wbuf;

	priv = (struct aicupg_mmc_priv *)fwc->priv;
	if (!priv) {
		pr_err("MMC FWC get priv failed.\n");
		goto out;
	}

	if (priv->remain_len > 0) {
		memcpy(wbuf, priv->remain_data, priv->remain_len);
		memcpy(wbuf + priv->remain_len, buf, len);
	} else {
		memcpy(wbuf, buf, len);
	}

	total_len = (priv->remain_len + len);
	remain = total_len;

	part_info = &(priv->part_info);

	if (mmc_getwp(priv->mmc) == 1) {
		pr_err("Error: card is write protected!\n");
		goto out;
	}

	sheader = &(priv->sparse_header);
	if (is_sparse_image(wbuf)) {
		priv->is_sparse = 1;
		memcpy(sheader, wbuf, sizeof(sparse_header_t));
		wbuf += sheader->file_hdr_sz;
		clen += sheader->file_hdr_sz;
		remain -= sheader->file_hdr_sz;
		if (sheader->file_hdr_sz > sizeof(sparse_header_t)) {
			wbuf += (sheader->file_hdr_sz - sizeof(sparse_header_t));
			clen += (sheader->file_hdr_sz - sizeof(sparse_header_t));
			remain -= (sheader->file_hdr_sz - sizeof(sparse_header_t));
		}
		pr_info("=== Sparse Image Header ===\n");
		pr_info("magic: 0x%x\n", sheader->magic);
		pr_info("major_version: 0x%x\n", sheader->major_version);
		pr_info("minor_version: 0x%x\n", sheader->minor_version);
		pr_info("file_hdr_sz: %d\n", sheader->file_hdr_sz);
		pr_info("chunk_hdr_sz: %d\n", sheader->chunk_hdr_sz);
		pr_info("blk_sz: %d\n", sheader->blk_sz);
		pr_info("total_blks: %d\n", sheader->total_blks);
		pr_info("total_chunks: %d\n", sheader->total_chunks);
	}

	pr_debug("Flashing Sparse Image\n");

	/* Start processing chunks */
	for (chunk = priv->cur_chunk; chunk < sheader->total_chunks; chunk++) {
		/* Read and skip over chunk header */
		cheader = (chunk_header_t *)wbuf;

		if (cheader->chunk_type != CHUNK_TYPE_RAW) {
			pr_debug("=== Chunk Header ===\n");
			pr_debug("chunk_type: 0x%x\n", cheader->chunk_type);
			pr_debug("chunk_data_sz: 0x%x\n", cheader->chunk_sz);
			pr_debug("total_size: 0x%x\n", cheader->total_sz);
		}

		if (cheader->chunk_type != CHUNK_TYPE_RAW &&
				cheader->chunk_type != CHUNK_TYPE_FILL &&
				cheader->chunk_type != CHUNK_TYPE_DONT_CARE &&
				cheader->chunk_type != CHUNK_TYPE_CRC32) {
			cheader = &(priv->chunk_header);
			chunk_data_sz = priv->cur_chunk_remain_data_sz;
		} else {
			wbuf += sheader->chunk_hdr_sz;
			clen += sheader->chunk_hdr_sz;
			remain -= sheader->chunk_hdr_sz;
			memcpy(&(priv->chunk_header), cheader, sizeof(chunk_header_t));
			if (sheader->chunk_hdr_sz > sizeof(chunk_header_t)) {
				/*
				 * Skip the remaining bytes in a header that is longer
				 * than we expected.
				 */
				wbuf += (sheader->chunk_hdr_sz - sizeof(chunk_header_t));
				clen += (sheader->chunk_hdr_sz - sizeof(chunk_header_t));
				remain -= (sheader->chunk_hdr_sz - sizeof(chunk_header_t));
			}
			chunk_data_sz = ((u64)sheader->blk_sz) * cheader->chunk_sz;
			priv->cur_chunk_remain_data_sz = chunk_data_sz;
			priv->cur_chunk_burned_data_sz = 0;
		}


		chunk_blkcnt = DIV_ROUND_UP_ULL(chunk_data_sz, MMC_BLOCK_SIZE);
		remain_blkcnt = remain / MMC_BLOCK_SIZE;
		switch (cheader->chunk_type) {
		case CHUNK_TYPE_RAW:
			if (cheader->total_sz !=
				(sheader->chunk_hdr_sz + chunk_data_sz + priv->cur_chunk_burned_data_sz)) {
				pr_err("Bogus chunk size for chunk type Raw\n");
				goto out;
			}

			if (priv->blkstart + chunk_blkcnt > part_info->size) {
				pr_err("Request would exceed partition size!\n");
				goto out;
			}

			desc = mmc_get_blk_desc(priv->mmc);
			if (remain_blkcnt > chunk_blkcnt &&
					(remain - chunk_data_sz) >= 16) {
				blks = blk_dwrite(desc, part_info->start + priv->blkstart, chunk_blkcnt, wbuf);
				if (blks < chunk_blkcnt) { /* blks might be > blkcnt (eg. NAND bad-blocks) */
					pr_err("Write failed, block %lu[%u]\n", part_info->start + priv->blkstart, blks);
					goto out;
				}
				remain = remain - chunk_data_sz;
				priv->cur_chunk_remain_data_sz = 0;
				priv->cur_chunk_burned_data_sz += chunk_data_sz;
			} else {
				blks = blk_dwrite(desc, part_info->start + priv->blkstart, remain_blkcnt, wbuf);
				if (blks < remain_blkcnt) { /* blks might be > blkcnt (eg. NAND bad-blocks) */
					pr_err("Write failed, block %lu[%u]\n", part_info->start + priv->blkstart, blks);
					goto out;
				}
				priv->cur_chunk_remain_data_sz -= remain_blkcnt * MMC_BLOCK_SIZE;
				priv->cur_chunk_burned_data_sz += remain_blkcnt * MMC_BLOCK_SIZE;
				remain = remain % MMC_BLOCK_SIZE;
			}

			priv->blkstart += blks;
			total_blocks += blks;
			wbuf += blks * MMC_BLOCK_SIZE;
			clen += blks * MMC_BLOCK_SIZE;
			if (priv->cur_chunk_remain_data_sz > 0 && (remain > 0 && remain < MMC_BLOCK_SIZE))
				break_flag = 1;
			break;

		case CHUNK_TYPE_FILL:
			if (cheader->total_sz !=
				(sheader->chunk_hdr_sz + sizeof(uint32_t))) {
				pr_err("Bogus chunk size for chunk type FILL\n");
				goto out;
			}

			fill_buf = (uint32_t *)memalign(ARCH_DMA_MINALIGN, ROUNDUP(SPARSE_FILLBUF_SIZE, ARCH_DMA_MINALIGN));
			if (!fill_buf) {
				pr_err("Malloc failed for: CHUNK_TYPE_FILL\n");
				goto out;
			}

			fill_val = *(uint32_t *)wbuf;
			wbuf = (char *)wbuf + sizeof(uint32_t);
			clen += sizeof(uint32_t);
			remain -= sizeof(uint32_t);

			if (priv->blkstart + chunk_blkcnt > part_info->size) {
				pr_err("Request would exceed partition size!\n");
				goto out;
			}

			for (i = 0; i < (SPARSE_FILLBUF_SIZE / sizeof(fill_val)); i++)
				fill_buf[i] = fill_val;

			remain_blks = ROUNDUP(part_info->start + priv->blkstart, 0x400) - (part_info->start + priv->blkstart);
			if (chunk_blkcnt >= (remain_blks + 0x400) && fill_val == 0x0) { // 512K
				blks = blk_dwrite(desc, part_info->start + priv->blkstart, remain_blks, fill_buf);
				if (blks < remain_blks) { /* blks might be > j (eg. NAND bad-blocks) */
					pr_err("Write failed, block %lu[%d]\n", part_info->start + priv->blkstart, remain_blks);
					free(fill_buf);
					goto out;
				}
				priv->blkstart += blks;

				erase_group = (chunk_blkcnt - remain_blks) / 0x400;
				blks = blk_derase(desc, part_info->start + priv->blkstart, erase_group * 0x400);
				if (blks != (erase_group * 0x400)) { /* blks might be > j (eg. NAND bad-blocks) */
					pr_err("Erase failed, block %lu[%d]\n", part_info->start + priv->blkstart, erase_group * 0x400);
					free(fill_buf);
					goto out;
				}
				priv->blkstart += blks;

				redund_blks = chunk_blkcnt - remain_blks - (erase_group * 0x400);
				blks = blk_dwrite(desc, part_info->start + priv->blkstart, redund_blks, fill_buf);
				if (blks < redund_blks) { /* blks might be > j (eg. NAND bad-blocks) */
					pr_err("Write failed, block %lu[%d]\n", part_info->start + priv->blkstart, redund_blks);
					free(fill_buf);
					goto out;
				}
				priv->blkstart += blks;
			} else {
				fill_buf_num_blks = SPARSE_FILLBUF_SIZE / MMC_BLOCK_SIZE;
				for (i = 0; i < chunk_blkcnt;) {
					j = chunk_blkcnt - i;
					if (j > fill_buf_num_blks)
						j = fill_buf_num_blks;

					blks = blk_dwrite(desc, part_info->start + priv->blkstart, j, fill_buf);
					if (blks < j) { /* blks might be > j (eg. NAND bad-blocks) */
						pr_err("Write failed, block %lu[%d]\n", part_info->start + priv->blkstart, j);
						free(fill_buf);
						goto out;
					}
					priv->blkstart += blks;
					i += j;
				}
			}
			total_blocks += DIV_ROUND_UP_ULL(chunk_data_sz, sheader->blk_sz);

			free(fill_buf);
			break;

		case CHUNK_TYPE_DONT_CARE:
			priv->blkstart += chunk_blkcnt;
			total_blocks += cheader->chunk_sz;
			break;

		case CHUNK_TYPE_CRC32:
			if (cheader->total_sz != sheader->chunk_hdr_sz) {
				pr_err("Bogus chunk size for chunk type Dont Care\n");
				goto out;
			}
			total_blocks += cheader->chunk_sz;
			wbuf += chunk_data_sz;
			clen += chunk_data_sz;
			remain -= chunk_data_sz;
			break;

		default:
			printf("Unknown chunk type: %x\n", cheader->chunk_type);
			cheader = &(priv->chunk_header);
		}

		if (break_flag)
			break;
	}

	priv->remain_len = remain;
	if (priv->remain_len) {
		priv->cur_chunk = chunk;
		memcpy(priv->remain_data, wbuf, priv->remain_len);
	}

	fwc->trans_size += clen;

	pr_debug("%s, data len %d, trans len %d\n", __func__, len, fwc->trans_size);

out:
	if (p)
		free(p);
	return len;
}

s32 mmc_fwc_raw_write(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_mmc_priv *priv;
	lbaint_t blkstart, blkcnt;
	struct blk_desc *desc;
	u8 *rdbuf;
	s32 clen = 0, calc_len;
	long n;

	rdbuf = malloc(len);
	priv = (struct aicupg_mmc_priv *)fwc->priv;
	if (!priv) {
		pr_err("MMC FWC get priv failed.\n");
		goto out;
	}

	if (mmc_getwp(priv->mmc) == 1) {
		pr_err("Error: card is write protected!\n");
		goto out;
	}

	blkstart = fwc->trans_size / MMC_BLOCK_SIZE;
	blkcnt = len / MMC_BLOCK_SIZE;
	if (len % MMC_BLOCK_SIZE)
		blkcnt++;

	if ((blkstart + blkcnt) > priv->part_info.size) {
		pr_err("Data size exceed the partition size.\n");
		goto out;
	}

	desc = mmc_get_blk_desc(priv->mmc);
	n = blk_dwrite(desc, priv->part_info.start + blkstart, blkcnt, buf);
	clen = len;
	if (n != blkcnt) {
		pr_err("Error, write to partition %s failed.\n",
		      fwc->meta.partition);
		fwc->burn_result += 1;
		clen = n * MMC_BLOCK_SIZE;
		fwc->trans_size += clen;
	}
	// Read data to calc crc
	n = blk_dread(desc, priv->part_info.start + blkstart, blkcnt, rdbuf);
	if (n != blkcnt) {
		pr_err("Error, read from partition %s failed.\n",
		      fwc->meta.partition);
		fwc->burn_result += 1;
		clen = n * MMC_BLOCK_SIZE;
		fwc->trans_size += clen;
	}

	if ((fwc->meta.size - fwc->trans_size) < len)
		calc_len = fwc->meta.size % DEFAULT_BLOCK_ALIGNMENT_SIZE;
	else
		calc_len = len;

	fwc->calc_partition_crc = crc32(fwc->calc_partition_crc,
						rdbuf, calc_len);
#ifdef CONFIG_AICUPG_SINGLE_TRANS_BURN_CRC32_VERIFY
	fwc->calc_trans_crc = crc32(fwc->calc_trans_crc, buf, calc_len);
	if (fwc->calc_trans_crc != fwc->calc_partition_crc) {
		pr_err("calc_len:%d\n", calc_len);
		pr_err("crc err at trans len %u\n", fwc->trans_size);
		pr_err("trans crc:0x%x, partition crc:0x%x\n",
				fwc->calc_trans_crc, fwc->calc_partition_crc);
	}
#endif

	fwc->trans_size += clen;

	debug("%s, data len %d, trans len %d, calc len %d\n", __func__, len,
	      fwc->trans_size, calc_len);

out:
	free(rdbuf);
	return clen;
}

s32 mmc_fwc_data_write(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_mmc_priv *priv;

	priv = (struct aicupg_mmc_priv *)fwc->priv;
	if (!is_sparse_image(buf) && !priv->is_sparse) {
		pr_debug("Not a sparse image\n");
		return mmc_fwc_raw_write(fwc, buf, len);
	} else {
		pr_debug("A sparse image\n");
		return mmc_fwc_sparse_write(fwc, buf, len);
	}
}

s32 mmc_fwc_data_read(struct fwc_info *fwc, u8 *buf, s32 len)
{
	struct aicupg_mmc_priv *priv;
	lbaint_t blkstart, blkcnt, start, trans_size;
	struct blk_desc *desc;
	s32 clen = 0;
	long n;

	priv = (struct aicupg_mmc_priv *)fwc->priv;
	if (!priv) {
		pr_err("MMC FWC get priv failed.\n");
		return 0;
	}

	start = fwc->mpart.part.start / MMC_BLOCK_SIZE;
	trans_size = fwc->trans_size;
	blkstart = trans_size / MMC_BLOCK_SIZE;
	blkcnt = len / MMC_BLOCK_SIZE;
	if (len % MMC_BLOCK_SIZE)
		blkcnt++;

	if ((blkstart + blkcnt) > fwc->mpart.part.size) {
		pr_err("Data size exceed the partition size.\n");
		return 0;
	}

	desc = mmc_get_blk_desc(priv->mmc);
	n = blk_dread(desc, start + blkstart, blkcnt, buf);
	clen = len;
	if (n != blkcnt) {
		pr_err("Error, Read from partition %s failed.\n",
		      fwc->meta.partition);
		fwc->burn_result += 1;
		clen = n * MMC_BLOCK_SIZE;
		fwc->trans_size += clen;
	}

	fwc->trans_size += clen;
	fwc->calc_partition_crc = fwc->meta.crc;

	debug("%s, data len %d, trans len %d\n", __func__, len,
	      fwc->trans_size);
	return clen;
}

void mmc_fwc_data_end(struct fwc_info *fwc)
{
	struct aicupg_mmc_priv *priv;
	priv = (struct aicupg_mmc_priv *)fwc->priv;
	if (fwc->priv) {
		free(priv);
		fwc->priv = 0;
	}
}
