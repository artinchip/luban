// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 * Author: Jianfeng Li <jianfeng.li@artinchip.com>
 */

#include <common.h>
#include <artinchip/aicupg.h>
#include "upg_internal.h"

static s32 media_device_write(char *image_name, struct fwc_meta *pmeta)
{
	s32 ret;
	struct fwc_info *fwc;
	u8 *buf;
	s32 total_len = 0;
	int offset, write_once_size, len, remaining_size;
	loff_t actread;

	fwc = NULL;
	buf = NULL;
	fwc = (struct fwc_info *)malloc(sizeof(struct fwc_info));
	if (!fwc) {
		pr_err("Error: malloc fwc failed.\n");
		ret = -1;
		goto err;
	}
	memset((void *)fwc, 0, sizeof(struct fwc_info));

	printf("Firmware component: %s\n", pmeta->name);
	printf("    partition: %s programming ...\n", pmeta->partition);
	/*config fwc */
	fwc_meta_config(fwc, pmeta);

	/*start write data*/
	media_data_write_start(fwc);
	/*config write size once*/
	write_once_size = DATA_WRITE_ONCE_SIZE;
	if (write_once_size % fwc->block_size)
		write_once_size = (write_once_size / fwc->block_size) * fwc->block_size;

	/*malloc buf memory*/
	buf = (u8 *)memalign(ARCH_DMA_MINALIGN, write_once_size);
	if (!buf) {
		pr_err("Error: malloc  buf failed.\n");
		ret = -1;
		goto err;
	}
	memset((void *)buf, 0, write_once_size);

	offset = 0;
	while (offset < pmeta->size) {
		remaining_size = pmeta->size - offset;
		len = min(remaining_size, write_once_size);
		if (len % fwc->block_size)
			len = ((len / fwc->block_size) + 1) * fwc->block_size;

		ret = fat_read_file(image_name, (void *)buf,
				    pmeta->offset + offset, len, &actread);
		if (actread != len && actread != remaining_size) {
			printf("Error:read file failed!\n");
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
	printf("    partition: %s programming done\n", pmeta->partition);
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

/*fat upgrade function*/
s32 aicupg_fat_write(char *image_name, char *protection,
		     struct image_header_upgrade *header)
{
	struct fwc_meta *p;
	struct fwc_meta *pmeta;
	int i;
	int cnt;
	s32 ret;
	s32 write_len = 0;
	loff_t actread;

	pmeta = NULL;
	pmeta = (struct fwc_meta *)malloc(header->meta_size);
	if (!pmeta) {
		pr_err("Error: malloc for meta failed.\n");
		ret = -1;
		goto err;
	}
	memset((void *)pmeta, 0, header->meta_size);

	/*read meta info*/
	ret = fat_read_file(image_name, (void *)pmeta, header->meta_offset,
			    header->meta_size, &actread);
	if (actread != header->meta_size) {
		printf("Error:read file failed!\n");
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
		if (!strcmp(p->partition, "") ||
		    strstr(protection, p->partition)) {
			p++;
			continue;
		}

		ret = media_device_write(image_name, p);
		if (ret == 0) {
			pr_err("media device write data failed!\n");
			goto err;
		}

		p++;
		write_len += ret;
	}

	printf("All firmaware components programming done.\n");
	free(pmeta);
	return write_len;
err:
	if (pmeta)
		free(pmeta);
	return 0;
}

