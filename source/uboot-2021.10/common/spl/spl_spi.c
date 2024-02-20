// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2011 OMICRON electronics GmbH
 *
 * based on drivers/mtd/nand/raw/nand_spl_load.c
 *
 * Copyright (C) 2011
 * Heiko Schocher, DENX Software Engineering, hs@denx.de.
 */

#include <common.h>
#include <image.h>
#include <log.h>
#include <spi.h>
#include <spi_flash.h>
#include <errno.h>
#include <spl.h>
#include <dm.h>
#include <asm/global_data.h>

#ifdef CONFIG_AUTO_CALCULATE_PART_CONFIG
#include <generated/image_cfg_part_config.h>
#endif
#ifdef CONFIG_VIDEO_ARTINCHIP
#include <cpu_func.h>
#include <artinchip/artinchip_fb.h>
#include <artinchip_ve.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

static ulong spl_spi_fit_read(struct spl_load_info *load, ulong sector,
			      ulong count, void *buf)
{
	struct spi_flash *flash = load->dev;
	ulong ret;

	ret = spi_flash_read(flash, sector, count, buf);
	if (!ret)
		return count;
	else
		return 0;
}

unsigned int __weak spl_spi_get_uboot_offs(struct spi_flash *flash)
{
	return CONFIG_SYS_SPI_U_BOOT_OFFS;
}

#ifdef CONFIG_SPL_OS_BOOT
#ifdef CONFIG_VIDEO_ARTINCHIP
static int spi_flash_read_logo(struct spi_flash *flash, const char *name)
{
	unsigned char *fit, *dst;
	size_t data_size;
	const void *data;
	struct udevice *dev;
	int noffset, ret;

	ret = uclass_first_device(UCLASS_VIDEO, &dev);
	if (ret) {
		pr_err("Failed to find aicfb udevice\n");
		return ret;
	}

	fit = memalign(DECODE_ALIGN, LOGO_MAX_SIZE);
	if (!fit) {
		pr_err("Failed to malloc for logo image!\n");
		return -ENOMEM;
	}

	spi_flash_read(flash, CONFIG_LOGO_PART_OFFSET, LOGO_MAX_SIZE,
		       (void *)fit);

	noffset = fit_image_get_node(fit, name);
	if (noffset < 0) {
		printf("Failed to get %s node\n", name);
		ret = -EINVAL;
		goto out;
	}

	/* Load specific logo image */
	ret = fit_image_get_data(fit, noffset, &data, &data_size);
	if (ret < 0) {
		pr_err("Failed to get data\n");
		ret = -EINVAL;
		goto out;
	}

	dst = memalign(DECODE_ALIGN, data_size);
	if (!dst) {
		pr_err("Failed to alloc dst buf\n");
		ret = -ENOMEM;
		goto out;
	}
	memcpy(dst, data, data_size);
	flush_dcache_range((uintptr_t)dst, (uintptr_t)dst + data_size);

	if (dst[1] == 'P' || dst[2] == 'N' || dst[3] == 'G')
		aic_png_decode(dst, data_size);
	else
		pr_err("not support logo file format, need a png image\n");

out:
	aicfb_update_ui_layer(dev);
	aicfb_startup_panel(dev);

	if (fit)
		free(fit);
	if (dst)
		free(dst);
	return ret;
}
#endif

/*
 * Load the kernel, check for a valid header we can parse, and if found load
 * the kernel and then device tree.
 */
static int spi_load_image_os(struct spl_image_info *spl_image,
			     struct spi_flash *flash,
			     struct image_header *header)
{
	int err;

#ifdef CONFIG_VIDEO_ARTINCHIP
	spi_flash_read_logo(flash, "boot");
#endif
	spi_flash_read(flash, CONFIG_SYS_SPI_ARGS_OFFS,
		       CONFIG_SYS_SPI_ARGS_SIZE,
		       (void *)CONFIG_SYS_SPL_ARGS_ADDR);

	spi_flash_read(flash, CONFIG_SYS_SPI_KERNEL_OFFS, sizeof(*header),
		       (void *)header);

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = flash;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = 1;
		load.read = spl_spi_fit_read;
		return spl_load_simple_fit(spl_image, &load,
					   CONFIG_SYS_SPI_KERNEL_OFFS, header);
	} else {
		err = spl_parse_image_header(spl_image, header);
		if (err)
			return err;
		spi_flash_read(flash, CONFIG_SYS_SPI_KERNEL_OFFS,
			       spl_image->size, (void *)spl_image->load_addr);
	}
	return 0;
}
#endif

/*
 * The main entry for SPI booting. It's necessary that SDRAM is already
 * configured and available since this code loads the main U-Boot image
 * from SPI into SDRAM and starts it from there.
 */
static int spl_spi_load_image(struct spl_image_info *spl_image,
			      struct spl_boot_device *bootdev)
{
	int err = 0;
	unsigned int payload_offs;
	struct spi_flash *flash;
	struct image_header *header;
	char num_bus = 0, cur_bus = 0;

	spl_image->flags |= SPL_COPY_PAYLOAD_ONLY;
	/*
	 * Load U-Boot image from SPI flash into RAM
	 * In DM mode: defaults speed and mode will be
	 * taken from DT when available
	 */

	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
				CONFIG_SF_DEFAULT_CS,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		num_bus = uclass_id_count(UCLASS_SPI);
		for (cur_bus = 0; !flash; cur_bus++) {
			if (cur_bus == CONFIG_SF_DEFAULT_BUS)
				continue;
			flash = spi_flash_probe(cur_bus,
						CONFIG_SF_DEFAULT_CS,
						CONFIG_SF_DEFAULT_SPEED,
						CONFIG_SF_DEFAULT_MODE);
			if (cur_bus >= num_bus) {
				puts("SPI probe failed.\n");
				return -ENODEV;
			}
		}
	}

	payload_offs = spl_spi_get_uboot_offs(flash);

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));

#if CONFIG_IS_ENABLED(OF_CONTROL) && !CONFIG_IS_ENABLED(OF_PLATDATA)
	payload_offs = fdtdec_get_config_int(gd->fdt_blob,
					     "u-boot,spl-payload-offset",
					     payload_offs);
#endif

#ifdef CONFIG_SPL_OS_BOOT
	if (!spl_start_uboot()) {
		err = spi_load_image_os(spl_image, flash, header);
		/* Double check after linux image is loaded. */
		if (!err && !spl_start_uboot())
			return 0;
	}
#endif
	{
		/* Load u-boot, mkimage header is 64 bytes. */
		err = spi_flash_read(flash, payload_offs, sizeof(*header),
				     (void *)header);
		if (err) {
			debug("%s: Failed to read from SPI flash (err=%d)\n",
			      __func__, err);
			return err;
		}

		if (IS_ENABLED(CONFIG_SPL_LOAD_FIT_FULL) &&
		    image_get_magic(header) == FDT_MAGIC) {
			err = spi_flash_read(flash, payload_offs,
					     roundup(fdt_totalsize(header), 4),
					     (void *)CONFIG_SYS_LOAD_ADDR);
			if (err)
				return err;
			err = spl_parse_image_header(spl_image,
					(struct image_header *)CONFIG_SYS_LOAD_ADDR);
		} else if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
			   image_get_magic(header) == FDT_MAGIC) {
			struct spl_load_info load;

			debug("Found FIT\n");
			load.dev = flash;
			load.priv = NULL;
			load.filename = NULL;
			load.bl_len = 1;
			load.read = spl_spi_fit_read;
			err = spl_load_simple_fit(spl_image, &load,
						  payload_offs,
						  header);
		} else if (IS_ENABLED(CONFIG_SPL_LOAD_IMX_CONTAINER)) {
			struct spl_load_info load;

			load.dev = flash;
			load.priv = NULL;
			load.filename = NULL;
			load.bl_len = 1;
			load.read = spl_spi_fit_read;

			err = spl_load_imx_container(spl_image, &load,
						     payload_offs);
		} else {
			err = spl_parse_image_header(spl_image, header);
			if (err)
				return err;
			err = spi_flash_read(flash, payload_offs + spl_image->offset,
					     spl_image->size,
					     (void *)spl_image->load_addr);
		}
	}

	return err;
}
/* Use priorty 1 so that boards can override this */
SPL_LOAD_IMAGE_METHOD("SPI", 1, BOOT_DEVICE_SPI, spl_spi_load_image);
