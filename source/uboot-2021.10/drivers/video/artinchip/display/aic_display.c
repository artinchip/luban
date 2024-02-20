// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021 ArtInChip Technology Co.,Ltd
 * Huahui Mai <huahui.mai@artinchip.com>
 */

#include <common.h>
#include <dm.h>
#include <video.h>
#include <video_console.h>
#include <bmp_layout.h>
#include <dm/device_compat.h>
#include <asm/unaligned.h>
#include <artinchip/artinchip_fb.h>
#include <artinchip_ve.h>
#include <asm/arch/boot_param.h>
#include <image.h>
#include <mmc.h>
#include <spi.h>
#include <spi_flash.h>
#include <mtd.h>
#include <fat.h>
#include <cpu_func.h>
#include <dm/uclass-internal.h>
#include <config_parse.h>

#include "aic_com.h"

#define IMAGE_HEADER_SIZE	(4 << 10)
#define CONFIG_LOGO_ITB_ADDRESS	0x42400000
#define BOOTCFG_FILE_SIZE	1024

#ifdef AICFB_CPU_DRAW
void aicfb_draw_text(uint x_frac, uint y, int value)
{
	struct udevice *dev;
	char str[25];
	int i;

	if (uclass_first_device_err(UCLASS_VIDEO_CONSOLE, &dev)) {
		debug("Failed to find video console dev\n");
		return;
	}

	struct vidconsole_priv *priv = dev_get_uclass_priv(dev);
	struct udevice *vid = dev->parent;
	struct video_priv *vid_priv = dev_get_uclass_priv(vid);

	priv->ycur = y;
	priv->xcur_frac = x_frac * 256;
	vid_priv->colour_bg = vid_console_color(vid_priv, VID_BLACK);
	vid_priv->colour_fg = vid_console_color(vid_priv, VID_LIGHT_BLUE);

	sprintf(str, "Upgrading %d%%", value);

	for (i = 0; str[i] != '\0'; i++)
		vidconsole_put_char(dev, str[i]);

	video_sync(dev->parent, false);
}

void aicfb_draw_rect(struct udevice *dev,
			uint x, uint y, uint width, uint height,
			u8 red, u8 green, u8 blue)
{
	struct video_priv *priv = dev_get_uclass_priv(dev);
	struct aicfb_dt *dt = dev_get_plat(dev);
	int pbytes = dt->format->bits_per_pixel / 8;
	uchar *fb;
	int i, j;

	fb = (uchar *)(priv->fb + y * priv->line_length + x * pbytes);

	switch (dt->format->format) {
	case AIC_FMT_RGB_888:
		for (i = 0; i < height; ++i) {
			for (j = 0; j < width; j++) {
				*(fb++) = blue;
				*(fb++) = green;
				*(fb++) = red;
			}
			fb += priv->line_length - width * pbytes;
		}
		break;
	case AIC_FMT_ARGB_8888:
		for (i = 0; i < height; ++i) {
			for (j = 0; j < width; j++) {
				*(fb++) = blue;
				*(fb++) = green;
				*(fb++) = red;
				*(fb++) = 0xFF;
			}
			fb += priv->line_length - width * pbytes;
		}
		break;
	case AIC_FMT_RGB_565:
		for (i = 0; i < height; ++i) {
			for (j = 0; j < width; j++) {
				*(uint16_t *)fb = ((red >> 3) << 11)
						| ((green >> 2) << 5)
						| (blue >> 3);
				fb += sizeof(uint16_t) / sizeof(*fb);
			}
			fb += priv->line_length - width * pbytes;
		}
		break;
	default:
		debug("%s: unsupported fb format %d", __func__,
						(int)dt->format->format);
		break;
	};

	video_sync(dev, false);
}

int aic_bmp_display(struct udevice *dev, ulong bmp_image)
{
	struct aicfb_dt *plat = dev_get_plat(dev);
	struct video_uc_plat *uplat = dev_get_uclass_plat(dev);
	uchar *fb, *bmap;
	struct bmp_image *bmp = (struct bmp_image *)bmp_image;
	int width, height, line_length;
	unsigned int bpix, bmp_bpix, byte_width, padded_byte;
	int i, x, y;

	if (!bmp || !(bmp->header.signature[0] == 'B' &&
				bmp->header.signature[1] == 'M')) {
		dev_err(dev, "no valid bmp image at %lx\n", bmp_image);

		return -EINVAL;
	}

	width = get_unaligned_le32(&bmp->header.width);
	height = get_unaligned_le32(&bmp->header.height);
	bmp_bpix = get_unaligned_le16(&bmp->header.bit_count);

	bpix = plat->format->bits_per_pixel;
	if (bpix != bmp_bpix) {
		dev_err(dev, "%d bit/pixel mode, but BMP has %d bit/pixel\n",
				bpix, bmp_bpix);
		return -EINVAL;
	}

	if (width > plat->width || height > plat->height) {
		dev_err(dev, "Video buffer %d x %d y"
			" but BMP has %d x %d y\n",
			plat->width, plat->height,
			width, height);
		return -EINVAL;
	}

	line_length = plat->stride;
	x = (plat->width - width) / 2;
	y = (plat->height - height) / 2;

	byte_width = width * (bmp_bpix / 8);
	padded_byte = (byte_width & 0x3 ? 4 - (byte_width & 0x3) : 0);

	bmap = (uchar *)bmp + get_unaligned_le32(&bmp->header.data_offset);
	fb = (uchar *)(uplat->base +
			(y + height) * line_length + x * bpix / 8);

	for (i = 0; i < height; ++i) {
		memcpy(fb, bmap, byte_width);

		bmap += byte_width + padded_byte;
		fb -= line_length;
	}

	video_sync(dev, false);

	return 0;
}
#endif

static int aic_logo_decode(unsigned char *dst, unsigned int size)
{
	if (dst[0] == 0xff || dst[1] == 0xd8) {
		pr_debug("Loaded a JPEG logo image\n");
		return aic_jpeg_decode(dst, size);
	}

	if (dst[1] == 'P' || dst[2] == 'N' || dst[3] == 'G') {
		pr_debug("Loaded a PNG logo image\n");
		return aic_png_decode(dst, size);
	}

	pr_err("not support logo file format, need a png/jpg image\n");
	return 0;
}

static int fit_image_get_node_prop(const void *fit, const char *name,
			const void **data, size_t *size)
{
	int noffset, ret;

	/* Get specific logo image offset */
	noffset = fit_image_get_node(fit, name);
	if (noffset < 0) {
		pr_err("Failed to get %s node\n", name);
		return -1;
	}

	/* Load specific logo image */
	ret = fit_image_get_data(fit, noffset, data, size);
	if (ret < 0) {
		pr_err("Failed to get data\n");
		return -1;
	}

	return 0;
}

static int fat_load_logo(const char *name)
{
	char *file_buf = NULL, *logo_itb = NULL;
	ulong offset, maxsize = 0;
	char imgname[IMG_NAME_MAX_SIZ];
	loff_t actread;
	size_t data_size;
	const void *data;
	unsigned char *dst;
	struct udevice *dev;
	int ret = -1;

	file_buf = (char *)malloc(BOOTCFG_FILE_SIZE);
	if (!file_buf) {
		pr_err("Error, malloc bootcfg.txt buf failed.\n");
		return ret;
	}
	memset((void *)file_buf, 0, BOOTCFG_FILE_SIZE);

	/* load bootcfg file */
	ret = fat_read_file("bootcfg.txt", (void *)file_buf, 0,
			BOOTCFG_FILE_SIZE, &actread);
	if (actread == 0 || ret != 0) {
		pr_err("Error:read file bootcfg.txt failed!\n");
		return ret;
	}

	/* Get logo part size and offset  */
	ret = boot_cfg_parse_file(file_buf, actread, "logo", imgname,
				  IMG_NAME_MAX_SIZ, &offset, &maxsize);
	if (ret <= 0) {
		pr_err("Parse boot cfg file failed.\n");
		return ret;
	}

	logo_itb = (char *)malloc(maxsize);
	if (!logo_itb) {
		pr_err("Error, malloc logo itb failed.\n");
		return -1;
	}

	/* Load logo itb */
	ret = fat_read_file(imgname, (void *)logo_itb,
			offset, maxsize, &actread);
	if (actread == 0 || ret != 0) {
		printf("Error:read file bootcfg.txt failed!\n");
		return ret;
	}

	ret = fit_image_get_node_prop(logo_itb, name, &data, &data_size);
	if (ret)
		goto out;

	dst = memalign(DECODE_ALIGN, data_size);
	if (!dst) {
		pr_err("Failed to alloc dst buf\n");
		return -1;
	}
	memcpy(dst, data, data_size);
	flush_dcache_range((uintptr_t)dst, (uintptr_t)dst + data_size);

	ret = uclass_find_first_device(UCLASS_VIDEO, &dev);
	if (ret) {
		pr_err("Failed to find aicfb udevice\n");
		return ret;
	}

	aic_logo_decode(dst, data_size);
	mdelay(16);
out:
	if (file_buf)
		free(file_buf);
	if (logo_itb)
		free(logo_itb);
	if (dst)
		free(dst);

	return ret;
}


static int spinand_load_logo(const char *name)
{
	int ret = 0;
#ifdef CONFIG_MTD
	unsigned int header_len, first_block, other_block;
	unsigned char *dst = NULL, *fit = NULL;
	const struct fdt_property *fdt_prop;
	int i, offset, next_offset;
	struct mtd_info *mtd;
	int tag = FDT_PROP;
	fdt32_t image_len;
	size_t retlen;

	fit = memalign(DECODE_ALIGN, IMAGE_HEADER_SIZE);
	if (!fit) {
		pr_err("Failed to malloc for logo header!\n");
		ret = -ENOMEM;
		goto out;
	}

	mtd_probe_devices();

	mtd = get_mtd_device_nm("logo");
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("MTD partition %s not found, ret %ld\n", "logo",
		       PTR_ERR(mtd));
		return PTR_ERR(mtd);
	}

	ret = mtd_read(mtd, 0, IMAGE_HEADER_SIZE, &retlen, fit);
	if (ret) {
		pr_err("Failed to read logo itb header for SPINAND\n");
		goto out;
	}

	offset = fit_image_get_node(fit, name);
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
		pr_err("Failed to malloc for logo image!\n");
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
	flush_dcache_range((uintptr_t)dst, (uintptr_t)dst + first_block);

	if (other_block) {
		ret = mtd_read(mtd, IMAGE_HEADER_SIZE, other_block, &retlen,
				dst + first_block);
		if (ret) {
			pr_err("Failed to read logo image for SPINAND\n");
			goto out;
		}
	}

	aic_logo_decode(dst, image_len);
out:
	put_mtd_device(mtd);
	if (dst)
		free(dst);
	if (fit)
		free(fit);
#endif
	return ret;
}

static int mmc_load_logo(const char *name, int id)
{
#ifdef CONFIG_MMC
	struct mmc *mmc = find_mmc_device(id);
	struct disk_partition part_info;
	struct udevice *dev;
	unsigned char *fit, *dst;
	size_t data_size;
	const void *data;
	int ret;

	ret = uclass_first_device(UCLASS_VIDEO, &dev);
	if (ret) {
		pr_err("Failed to find aicfb udevice\n");
		return ret;
	}

	ret = part_get_info_by_name(mmc_get_blk_desc(mmc), "logo", &part_info);
	if (ret < 0) {
		pr_err("Get logo partition information failed.\n");
		return -EINVAL;
	}

	fit = memalign(DECODE_ALIGN, part_info.blksz * part_info.size);
	if (!fit) {
		pr_err("Failed to malloc for fit image!\n");
		return -ENOMEM;
	}

	ret = blk_dread(mmc_get_blk_desc(mmc), part_info.start,
						part_info.size, fit);
	if (ret != part_info.size) {
		pr_err("Failed to read logo image from MMC/SD!\n");
		ret = -EIO;
		goto out;
	}

	ret = fit_image_get_node_prop(fit, name, &data, &data_size);
	if (ret) {
		pr_err("Failed to get fit image prop\n");
		ret = -EINVAL;
		goto out;
	}

	dst = memalign(DECODE_ALIGN, data_size);
	if (!dst) {
		pr_err("Failed to malloc dst buffer\n");
		ret = -ENOMEM;
		goto out;
	}
	memcpy(dst, data, data_size);
	flush_dcache_range((uintptr_t)dst, (uintptr_t)dst + data_size);

	aic_logo_decode(dst, data_size);
out:
	if (fit)
		free(fit);
	if (dst)
		free(dst);
#endif
	return ret;
}

static int bootrom_load_logo(const char *name)
{
	const void *fit = (void *)CONFIG_LOGO_ITB_ADDRESS;
	const void *data;
	size_t size;
	char *dst;
	int ret;

	ret = fit_image_get_node_prop(fit, name, &data, &size);
	if (ret)
		return -EINVAL;

	dst = memalign(DECODE_ALIGN, size);
	if (!dst) {
		pr_err("Failed to malloc dst buffer\n");
		return -EINVAL;
	}
	memcpy(dst, data, size);
	flush_dcache_range((uintptr_t)dst, (uintptr_t)dst + size);

	aic_logo_decode(dst, size);
	free(dst);
	return 0;
}

static int spinor_load_logo(const char *name)
{
#ifdef CONFIG_DM_SPI_FLASH
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	unsigned char *fit = NULL, *dst = NULL;
	struct udevice *new, *bus_dev;
	struct spi_flash *flash;
	size_t data_size;
	const void *data;
	int ret;

	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (ret) {
		pr_err("Failed to find a spi device\n");
		return -EINVAL;
	}
	flash = dev_get_uclass_priv(new);

	fit = memalign(DECODE_ALIGN, LOGO_MAX_SIZE);
	if (!fit) {
		printf("Failed to malloc for logo image!\n");
		return -ENOMEM;
	}

	ret = spi_flash_read(flash, CONFIG_LOGO_PART_OFFSET, LOGO_MAX_SIZE, fit);
	if (ret) {
		printf("Failed to read logo image for SPINOR\n");
		ret = -EINVAL;
		goto out;
	}

	ret = fit_image_get_node_prop(fit, name, &data, &data_size);
	if (ret) {
		printf("Failed to get fit image prop\n");
		ret = -EINVAL;
		goto out;
	}

	dst = memalign(DECODE_ALIGN, data_size);
	if (!dst) {
		printf("Failed to alloc dst buf\n");
		ret = -ENOMEM;
		goto out;
	}
	memcpy(dst, data, data_size);
	flush_dcache_range((uintptr_t)dst, (uintptr_t)dst + data_size);

	aic_logo_decode(dst, data_size);
out:
	if (fit)
		free(fit);
	if (dst)
		free(dst);
#endif
	return 0;
}

int aic_disp_logo(const char *name, int boot_param)
{
	int ret = 0;

	switch (boot_param) {
	case BD_SDMC0:
		ret = mmc_load_logo(name, 0);
		break;
	case BD_SDMC1:
		ret = mmc_load_logo(name, 1);
		break;
	case BD_SPINAND:
		ret = spinand_load_logo(name);
		break;
	case BD_SPINOR:
		ret = spinor_load_logo(name);
		break;
	case BD_SDFAT32:
		ret = fat_load_logo(name);
		break;
	case BD_BOOTROM:
		ret = bootrom_load_logo(name);
		break;
	default:
		pr_err("Do not support boot device id: %d\n", boot_param);
		return -EINVAL;
	}

	return ret;
}
