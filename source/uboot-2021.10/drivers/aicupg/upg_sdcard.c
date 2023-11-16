// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Author: Jianfeng Li <jianfeng.li@artinchip.com>
 */

#include <common.h>
#include <artinchip/aicupg.h>
#include "upg_internal.h"

static s32 media_device_write(struct fwc_meta *pmeta, struct mmc *mmc,
			      struct disk_partition part_info)
{
	s32 ret;
	struct fwc_info *fwc;
	u8 *buf;
	u32 len;
	u32 write_once_size, offset;
	s32 total_len = 0;
	u32 blk_offset, blk_cnt, blk_n, meta_blk_offset;

	fwc = NULL;
	buf = NULL;
	fwc = (struct fwc_info *)malloc(sizeof(struct fwc_info));
	if (!fwc) {
		pr_err("Error, malloc for meta failed.\n");
		ret = -1;
		goto err;
	}
	memset((void *)fwc, 0, sizeof(struct fwc_info));

	/*config fwc */
	fwc_meta_config(fwc, pmeta);

	/*start write data*/
	media_data_write_start(fwc);
	/*config write size once*/
	write_once_size = DATA_WRITE_ONCE_SIZE;
	if (write_once_size % fwc->block_size) {
		write_once_size = (write_once_size / fwc->block_size) * fwc->block_size;
	}
	/*malloc buf memory*/
	buf = (u8 *)malloc(write_once_size + mmc->read_bl_len);
	if (!buf) {
		pr_err("Error, malloc  buf failed.\n");
		ret = -1;
		goto err;
	}
	memset((void *)buf, 0, write_once_size + mmc->read_bl_len);
	meta_blk_offset = part_info.start + (pmeta->offset / mmc->read_bl_len);
	offset = 0;
	while (offset < pmeta->size) {
		len = min(pmeta->size - offset, write_once_size);
		if (len % mmc->read_bl_len)
			blk_cnt = (len / mmc->read_bl_len) + 1;
		else
			blk_cnt = len / mmc->read_bl_len;
		/*always read the whole block*/
		if ((blk_cnt * mmc->read_bl_len) % fwc->block_size) {
			blk_cnt += (((fwc->block_size - ((blk_cnt * mmc->read_bl_len) % \
			fwc->block_size))) / mmc->read_bl_len) + 1;
		}
		blk_offset = offset / mmc->read_bl_len;
		blk_n = blk_dread(mmc_get_blk_desc(mmc), meta_blk_offset + blk_offset,\
				  blk_cnt, (void *)buf);
		if (blk_n != blk_cnt) {
			pr_err("load partition:%s data failed!\n", pmeta->partition);
			goto err;
		}
		/*write data to media*/
		ret = media_data_write(fwc, buf, len);
		if (ret == 0) {
			pr_err("Error: media write failed!..\n");
			goto err;
		}
		total_len += ret;
		offset += len;
	}
	/*write data end*/
	media_data_write_end(fwc);
	/*check data */

	if (buf)
		free(buf);
	if (fwc)
		free(fwc);
	return total_len;
err:
	if (buf)
		free(buf);
	if (fwc)
		free(fwc);
	return 0;

}


/*write function*/
s32 aicupg_sd_write(struct image_header_upgrade *header, struct mmc *mmc,
		    struct disk_partition part_info)
{
	struct fwc_meta *p;
	struct fwc_meta *pmeta;
	int i;
	int cnt;
	s32 ret;
	s32 write_len = 0;
	u32 blk_meta_offset, blk_cnt, blk_n;

	pmeta = NULL;
	pmeta = (struct fwc_meta *)malloc(header->meta_size);
	if (!pmeta) {
		pr_err("Error, malloc for meta failed.\n");
		ret = -1;
		goto err;
	}
	memset((void *)pmeta, 0, header->meta_size);

	blk_meta_offset = header->meta_offset / mmc->read_bl_len;
	blk_cnt = header->meta_size / mmc->read_bl_len;
	blk_n = blk_dread(mmc_get_blk_desc(mmc), part_info.start + blk_meta_offset,\
		blk_cnt, (void *)pmeta);
	if (blk_n != blk_cnt) {
		pr_err("load header failed!\n");
		goto err;
	}

	/*prepare device*/
	ret = media_device_prepare(NULL, header);
	if (ret != 0) {
		pr_err("Error:prepare device failed!\n");
		goto err;
	}

	p = pmeta;
	cnt = header->meta_size / sizeof(struct fwc_meta);
	for (i = 0; i < cnt; i++) {
		if (strcmp(p->partition, "") == 0) {
			p++;
			continue;
		}

		ret = media_device_write(p, mmc, part_info);
		if (ret == 0) {
			pr_err("media device write data failed!\n");
			goto err;
		}

		p++;
		write_len += ret;
	}

	free(pmeta);
	return write_len;
err:
	if (pmeta)
		free(pmeta);
	return 0;
}

