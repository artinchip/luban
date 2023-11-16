// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2011 OMICRON electronics GmbH
 *
 * based on drivers/mtd/nand/raw/nand_spl_load.c
 *
 * Copyright (C) 2011
 * Heiko Schocher, DENX Software Engineering, hs@denx.de.
 *
 * Copyright (c) 2022, ArtInChip Technology Co., Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
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
#include <artinchip_spinand.h>
#include <linux/mtd/spinand.h>
#include <cpu_func.h>
#include <dm/uclass-internal.h>
#include <artinchip_ve.h>
#include <artinchip/artinchip_fb.h>

DECLARE_GLOBAL_DATA_PTR;

struct spinand_device *spl_spinand_init(void)
{
	struct spinand_device *spinand;
	struct udevice *dev;
	int err = -1;

	spinand = get_spinand();
	if (spinand)
		return spinand;

	err = uclass_first_device(UCLASS_MTD, &dev);
	if (err && !dev) {
		pr_err("Find MTD device failed.\n");
		return NULL;
	}

	device_probe(dev);
	spinand = get_spinand();

	return spinand;
}

static bool aligned_with_block_size(struct nand_device *nand, u64 size)
{
	return !do_div(size, nand->info->erasesize);
}

int spl_spi_nand_read(struct spinand_device *spinand, size_t from, size_t len,
		      size_t *retlen, u_char *buf)
{
	size_t remaining, off, rdlen;
	int ret = 0;
	struct nand_device *nand = spinand_to_nand(spinand);

	/* Search for the first good block after the given offset */
	off = from;
	remaining = len;
	while (spinand_block_isbad(spinand, off))
		off += nand->info->erasesize;

	while (remaining) {
		/*
		 * aligned with block size and skip bad block
		 */
		if (aligned_with_block_size(nand, off) &&
		    spinand_block_isbad(spinand, off)) {
			off += nand->info->erasesize;
			continue;
		}
		/* Read one block per loop, so that it can skip bad blocks  */
		if (remaining > nand->info->erasesize)
			rdlen = nand->info->erasesize;
		else
			rdlen = remaining;
		/* Use continuous mode read all data at once */
#ifdef CONFIG_SPI_NAND_WINBOND_CONT_READ
			rdlen = remaining;
#endif
		spinand_read(spinand, off, rdlen, &rdlen, buf);
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
	struct spinand_device *spinand;
	size_t retlen;
	int ret;

	spinand = (struct spinand_device *)load->dev;
	ret = spl_spi_nand_read(spinand, offs, size, &retlen, (u_char *)dst);
	if (!ret)
		return (ulong)retlen;
	else
		return 0;
}

static __maybe_unused
int spi_nand_load_image(struct spl_image_info *spl_image,
			struct spinand_device *spinand, ulong offset)
{
	int *src __attribute__((unused));
	int *dst __attribute__((unused));
	struct image_header *header;
	size_t retlen;
	int err = -1;

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));

	err = spl_spi_nand_read(spinand, offset, sizeof(*header), &retlen,
				(u_char *)header);

	if (err) {
		debug("Read image header failed.\n");
		return err;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = (void *)spinand;
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
		err = spl_spi_nand_read(spinand, offset, spl_image->size, &retlen,
					(u_char *)spl_image->load_addr);
		return err;
	}

	return 0;
}

#ifdef CONFIG_SPL_OS_BOOT
#ifdef CONFIG_VIDEO_ARTINCHIP
#define LOGO_OFFSET	(0x340000)
#define IMAGE_HEADER_SIZE (4 << 10)

static int spi_nand_load_logo(struct spinand_device *spinand,
				unsigned char **buf, size_t *len)
{
	unsigned char *dst = NULL, *fit = NULL;
	unsigned int header_len, first_block, other_block;
	const struct fdt_property *fdt_prop;
	int i, offset, next_offset;
	int tag = FDT_PROP;
	fdt32_t image_len;
	size_t retlen;
	int ret = 0;

	fit = memalign(DECODE_ALIGN, IMAGE_HEADER_SIZE);
	if (!fit) {
		pr_err("Failed to malloc for logo header!\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = spl_spi_nand_read(spinand, LOGO_OFFSET, IMAGE_HEADER_SIZE,
				&retlen, fit);
	if (ret) {
		pr_err("Failed to read logo header\n");
		goto out;
	}

	offset = fit_image_get_node(fit, "boot");
	if (offset < 0) {
		pr_err("Failed to find boot node in logo itb\n");
		goto out;
	}

	/* Find data property */
	for (i = 0; i < 4; i++) {
		tag = fdt_next_tag(fit, offset, &next_offset);
		if (tag == FDT_END)
			goto out;

		offset = next_offset;
	}
	fdt_prop = fdt_get_property_by_offset(fit, offset, NULL);
	image_len = fdt32_to_cpu(fdt_prop->len);

	header_len = (phys_addr_t)fdt_prop - (phys_addr_t)fit
					+ sizeof(struct fdt_property);

	dst = memalign(DECODE_ALIGN, ALIGN(image_len, IMAGE_HEADER_SIZE));
	if (!dst) {
		pr_err("Failed to malloc for boot logo image\n");
		ret = -ENOMEM;
		goto out;
	}

	if (image_len < IMAGE_HEADER_SIZE) {
		first_block = image_len;
		other_block = 0;
	} else {
		first_block = IMAGE_HEADER_SIZE - header_len;
		other_block = ALIGN(image_len - first_block, IMAGE_HEADER_SIZE);
	}
	memcpy(dst, fit + header_len, first_block);

	if (other_block) {
		ret = spl_spi_nand_read(spinand, LOGO_OFFSET + IMAGE_HEADER_SIZE,
				other_block, &retlen, dst + first_block);
		if (ret) {
			pr_err("Failed to read boot logo other block\n");
			goto out;
		}
	}
	flush_dcache_range((uintptr_t)dst, (uintptr_t)dst + image_len);

	*buf = dst;
	*len = image_len;
	return 0;

out:
	if (dst)
		free(dst);
	if (fit)
		free(fit);
	return ret;
}

static int spi_nand_show_logo(struct spinand_device *spinand)
{
	struct udevice *dev;
	unsigned char *dst = NULL;
	size_t len = 0;
	int ret;

	ret = uclass_first_device(UCLASS_VIDEO, &dev);
	if (ret) {
		pr_err("Failed to find display udevice\n");
		return ret;
	}

	ret = spi_nand_load_logo(spinand, &dst, &len);
	if (ret)
		goto out;

	if (dst[1] == 'P' || dst[2] == 'N' || dst[3] == 'G')
		aic_png_decode(dst, len);
	else if (dst[0] == 0xff || dst[1] == 0xd8)
		aic_jpeg_decode(dst, len);
	else
		pr_err("Invaild logo file format, need a png/jpeg image\n");

out:
	aicfb_update_ui_layer(dev);
	aicfb_startup_panel(dev);

	return ret;
}
#endif

static int spi_nand_load_image_os(struct spl_image_info *spl_image,
				  struct spinand_device *spinand, ulong offset)
{
	size_t retlen;
	int err = -1;

	/* load dtb from falcon partition in falcon mode */
	spl_spi_nand_read(spinand, CONFIG_SYS_SPL_NAND_OFS,
			  CONFIG_SYS_SPL_WRITE_SIZE, &retlen, (void *)
			  CONFIG_SYS_SPL_ARGS_ADDR);

#ifdef CONFIG_VIDEO_ARTINCHIP
	spi_nand_show_logo(spinand);
#endif

	err = spi_nand_load_image(spl_image, spinand, offset);
	if (err)
		return err;

	if (spl_image->os != IH_OS_LINUX && spl_image->os != IH_OS_TEE &&
	    spl_image->os != IH_OS_OPENSBI) {
		puts("Expected image is not found. Trying to start U-boot\n");
		return -ENOENT;
	}

	return 0;
}
#endif

static int spl_spi_nand_load_image(struct spl_image_info *spl_image,
				   struct spl_boot_device *bootdev)
{
	ulong offset = 0;
	int *src __attribute__((unused));
	int *dst __attribute__((unused));
	struct spinand_device *spinand;

	spinand = spl_spinand_init();
	if (IS_ERR_OR_NULL(spinand)) {
		pr_err("Tiny SPI NAND init failed., ret = %ld\n", PTR_ERR(spinand));
		return -1;
	}

#ifdef CONFIG_NAND_BBT_MANAGE
	spl_nand_bbt_init(spinand);
#endif

#ifdef CONFIG_SPL_OS_BOOT
	int ret = 0;
	if (!spl_start_uboot()) {
		/* load kernel addr in falcon mode */
		offset = CONFIG_SYS_NAND_SPL_KERNEL_OFFS;

		ret = spi_nand_load_image_os(spl_image, spinand, offset);

		/* Double check after linux image is loaded. */
		if (!ret && !spl_start_uboot())
			return 0;
	}
#endif
	offset = CONFIG_SYS_SPI_NAND_U_BOOT_OFFS;

	return spi_nand_load_image(spl_image, spinand, offset);
}

SPL_LOAD_IMAGE_METHOD("SPINAND", 0, BOOT_DEVICE_SPI, spl_spi_nand_load_image);
