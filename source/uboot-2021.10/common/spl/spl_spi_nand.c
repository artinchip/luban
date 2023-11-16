// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2011 OMICRON electronics GmbH
 *
 * based on drivers/mtd/nand/raw/nand_spl_load.c
 *
 * Copyright (C) 2011
 * Heiko Schocher, DENX Software Engineering, hs@denx.de.
 *
 * Copyright (c) 2021, ArtInChip Technology Co., Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <config.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <image.h>
#include <spi.h>
#include <mtd.h>
#include <spi_flash.h>
#include <errno.h>
#include <spl.h>
#include <nand.h>
#include <ubispl.h>
#include <spl.h>
#include <asm/arch/boot_param.h>

DECLARE_GLOBAL_DATA_PTR;

static struct mtd_info *spi_mtd;
#ifdef CONFIG_SPL_UBI
static u8 *scratch_buf;
#endif

static struct mtd_info *spl_spi_nand_init(void)
{
	struct mtd_info *mtd;
	struct udevice *dev;
	int err = -1;

	if (spi_mtd)
		return spi_mtd;

	err = uclass_first_device(UCLASS_MTD, &dev);
	if (err && !dev) {
		pr_err("Find MTD device failed.\n");
		return NULL;
	}

	device_probe(dev);
	mtd = get_mtd_device(NULL, 0);
	if (!mtd) {
		pr_err("Get SPI NAND mtd device failed.\n");
		return NULL;
	}

	spi_mtd = mtd;
	return spi_mtd;
}

#ifdef CONFIG_SPL_UBI
/**
 * nand_spl_read_block - Read data from physical eraseblock into a buffer
 * @block:	Number of the physical eraseblock
 * @offset:	Data offset from the start of @peb
 * @len:	Data size to read
 * @dst:	Address of the destination buffer
 *
 * This could be further optimized if we'd have a subpage read
 * function in the simple code. On NAND which allows subpage reads
 * this would spare quite some time to readout e.g. the VID header of
 * UBI.
 *
 * Notes:
 *	@offset + @len are not allowed to be larger than a physical
 *	erase block. No sanity check done for simplicity reasons.
 *
 * To support runtime detected flash this needs to be extended by
 * information about the actual flash geometry, but thats beyond the
 * scope of this effort and for most applications where fast boot is
 * required it is not an issue anyway.
 */
int nand_spl_read_block(int block, int offset, int len, void *dst)
{
	int page, pagesize, ret, off_in_page, read;
	struct mtd_info *mtd = spi_mtd;
	size_t rdlen;

	pagesize = mtd->writesize;
	if (!scratch_buf) {
		scratch_buf = malloc(pagesize);
		if (!scratch_buf) {
			pr_err("Malloc scratch buffer failed.\n");
			return -1;
		}
	}

	/* Calculate the real offset from start */
	offset = block * mtd->erasesize + offset;
	page = offset / pagesize;

	/* Offset to the start of a flash page */
	off_in_page = offset % pagesize;

	while (len) {
		/*
		 * Non page aligned reads go to the scratch buffer.
		 * Page aligned reads go directly to the destination.
		 */
		if (off_in_page || len < pagesize) {
			ret = mtd_read(mtd, (page * pagesize), pagesize, &rdlen,
				       scratch_buf);
			if (ret) {
				pr_err("Failure while reading at offset 0x%x\n",
				       page * pagesize);
				break;
			}
			read = min(len, pagesize - off_in_page);
			memcpy(dst, scratch_buf + off_in_page, read);
			off_in_page = 0;
		} else {
			ret = mtd_read(mtd, (page * pagesize), pagesize, &rdlen,
				       dst);
			if (ret) {
				pr_err("Failure while reading at offset 0x%x\n",
				       page * pagesize);
				break;
			}
			read = (int)rdlen;
		}
		page++;
		len -= read;
		dst += read;
	}

	return 0;
}

int spl_ubi_load_image(struct spl_image_info *spl_image,
		       struct spl_boot_device *bootdev)
{
	struct image_header *header;
	struct ubispl_info info;
	struct ubispl_load volumes[2];
	struct mtd_info *mtd;
	int ret = 1;

	mtd = spl_spi_nand_init();
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("SPI NAND init failed for UBI load image.\n");
		return -1;
	}

	switch (bootdev->boot_device) {
	case BOOT_DEVICE_SPI:
		info.read = nand_spl_read_block;
		info.peb_size = mtd->erasesize;
		break;
	default:
		goto out;
	}
	info.ubi = (struct ubi_scan_info *)CONFIG_SPL_UBI_INFO_ADDR;
	info.fastmap = IS_ENABLED(CONFIG_MTD_UBI_FASTMAP);

	info.peb_offset = CONFIG_SPL_UBI_PEB_OFFSET;
	info.vid_offset = CONFIG_SPL_UBI_VID_OFFSET;
	info.leb_start = CONFIG_SPL_UBI_LEB_START;
	info.peb_count = CONFIG_SPL_UBI_MAX_PEBS - info.peb_offset;

#ifdef CONFIG_SPL_OS_BOOT
	if (!spl_start_uboot()) {
		volumes[0].vol_id = CONFIG_SPL_UBI_LOAD_KERNEL_ID;
		volumes[0].load_addr = (void *)CONFIG_SYS_LOAD_ADDR;
		volumes[1].vol_id = CONFIG_SPL_UBI_LOAD_ARGS_ID;
		volumes[1].load_addr = (void *)CONFIG_SYS_SPL_ARGS_ADDR;

		ret = ubispl_load_volumes(&info, volumes, 2);
		if (!ret) {
			header = (struct image_header *)volumes[0].load_addr;
			spl_parse_image_header(spl_image, header);
			puts("Linux loaded.\n");
			goto out;
		}
		puts("Loading Linux failed, falling back to U-Boot.\n");
	}
#endif
	header = spl_get_load_buffer(-sizeof(*header), sizeof(header));
#ifdef CONFIG_SPL_UBI_LOAD_BY_VOLNAME
	volumes[0].vol_id = -1;
	strncpy(volumes[0].name,
		CONFIG_SPL_UBI_LOAD_MONITOR_VOLNAME,
		UBI_VOL_NAME_MAX + 1);
#else
	volumes[0].vol_id = CONFIG_SPL_UBI_LOAD_MONITOR_ID;
#endif
	volumes[0].load_addr = (void *)header;

	ret = ubispl_load_volumes(&info, volumes, 1);
	if (!ret)
		spl_parse_image_header(spl_image, header);
out:
	return ret;
}


#else

static bool aligned_with_block_size(struct mtd_info *mtd, u64 size)
{
	return !do_div(size, mtd->erasesize);
}

static int spl_spi_nand_read(struct mtd_info *mtd, size_t from, size_t len,
			     size_t *retlen, u_char *buf)
{
	size_t remaining, off, rdlen;
	int ret = 0;

	/* Search for the first good block after the given offset */
	off = from;
	remaining = len;
	while (mtd_block_isbad(mtd, off))
		off += mtd->erasesize;

	while (remaining) {
		/*
		 * aligned with block size and skip bad block
		 */
		if (aligned_with_block_size(mtd, off) &&
		    mtd_block_isbad(mtd, off)) {
			off += mtd->erasesize;
			continue;
		}
		/* Read one block per loop, so that it can skip bad blocks  */
		if (remaining > mtd->erasesize)
			rdlen = mtd->erasesize;
		else
			rdlen = remaining;
		ret = mtd_read(mtd, off, rdlen, &rdlen, buf);
		if (ret) {
			pr_err("Failure while reading at offset 0x%lx\n",
			       (unsigned long)off);
			break;
		}
		off += rdlen;
		remaining -= rdlen;
		buf += rdlen;
	}

	*retlen = len - remaining;
	return ret;
}

static ulong spl_spi_nand_fit_read(struct spl_load_info *load, ulong offs,
				   ulong size, void *dst)
{
	struct mtd_info *mtd;
	size_t retlen;
	int ret;

	mtd = (struct mtd_info *)load->dev;
	ret = spl_spi_nand_read(mtd, offs, size, &retlen, (u_char *)dst);
	if (!ret)
		return (ulong)retlen;
	else
		return 0;
}

static int spl_spi_nand_load_image(struct spl_image_info *spl_image,
				   struct spl_boot_device *bootdev)
{
	ulong offset = CONFIG_SYS_SPI_NAND_U_BOOT_OFFS;
	int *src __attribute__((unused));
	int *dst __attribute__((unused));
	struct image_header *header;
	struct mtd_info *mtd;
	size_t retlen;
	int err = -1;

	mtd = spl_spi_nand_init();
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("MTD SPI NAND init failed., ret = %ld\n", PTR_ERR(mtd));
		return err;
	}

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));

#ifdef CONFIG_SPL_OS_BOOT
	if (!spl_start_uboot()) {
		/*
		 * load parameter image
		 * load to temp position since nand_spl_load_image reads
		 * a whole block which is typically larger than
		 * CONFIG_CMD_SPL_WRITE_SIZE therefore may overwrite
		 * following sections like BSS
		 */
		spl_spi_nand_read(mtd, CONFIG_CMD_SPL_NAND_OFS,
				  CONFIG_CMD_SPL_WRITE_SIZE, &retlen,
				  (u_char *)CONFIG_SYS_TEXT_BASE);
		/* copy to destintion */
		dst = (int *)CONFIG_SYS_SPL_ARGS_ADDR;
		src = (int *)CONFIG_SYS_TEXT_BASE;
		memcpy(dst, src, CONFIG_CMD_SPL_WRITE_SIZE);

		/* load linux */
		spl_spi_nand_read(mtd, CONFIG_SYS_NAND_SPL_KERNEL_OFFS,
				  sizeof(*header), &retlen, (u_char *)header);
		err = spl_parse_image_header(spl_image, header);
		if (err)
			return err;
		if (header->ih_os == IH_OS_LINUX) {
			/* happy - was a linux */
			err = spl_spi_nand_read(mtd,
						CONFIG_SYS_NAND_SPL_KERNEL_OFFS,
						spl_image->size, &retlen,
						(u_char *)spl_image->load_addr);
			/* Double check after linux image is loaded. */
			if (spl_start_uboot())
				goto force_uboot;
			return err;
		} else {
			puts("The Expected Linux image was not "
				"found. Please check your NAND "
				"configuration.\n");
			puts("Trying to start u-boot now...\n");
		}
	}
force_uboot:
#endif
	offset = CONFIG_SYS_SPI_NAND_U_BOOT_OFFS;
	err = spl_spi_nand_read(mtd, offset, sizeof(*header), &retlen,
				(u_char *)header);

	if (err) {
		debug("Read image header failed.\n");
		return err;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = (void *)mtd;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = 1;
		load.read = spl_spi_nand_fit_read;
		return spl_load_simple_fit(spl_image, &load, offset, header);
	} else {
		err = spl_parse_image_header(spl_image, header);
		if (err) {
			debug("spl_parse_image_header failed.\n");
			return err;
		}
		err = spl_spi_nand_read(mtd, offset, spl_image->size, &retlen,
					(u_char *)spl_image->load_addr);
		return err;
	}
}
#endif

#ifdef CONFIG_SPL_UBI
/* Use priorty 0 to override other SPI device when this device is enabled. */
SPL_LOAD_IMAGE_METHOD("SPINAND_UBI", 0, BOOT_DEVICE_SPI, spl_ubi_load_image);
#else
SPL_LOAD_IMAGE_METHOD("SPINAND", 0, BOOT_DEVICE_SPI, spl_spi_nand_load_image);
#endif
