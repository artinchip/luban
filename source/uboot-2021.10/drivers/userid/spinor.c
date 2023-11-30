// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 ArtInChip Technology Co.,Ltd.
 */

#include <common.h>
#include <generated/autoconf.h>
#include <command.h>
#include <dm.h>
#include <dm/uclass.h>
#include <dm/device-internal.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <memalign.h>
#include <errno.h>
#include <flash.h>
#include <spi.h>
#include <spi_flash.h>
#include <userid.h>
#include "userid_internal.h"

#ifdef CONFIG_AUTO_CALCULATE_PART_CONFIG
#include <generated/image_cfg_part_config.h>
#endif

static int spinor_userid_load(void)
{
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	struct udevice *new, *bus_dev;
	struct spi_flash *flash;
	struct userid_storage_header head;
	size_t offset, size;
	int ret = -1;
	u8 *buf;

	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (ret) {
		pr_err("Failed to find a spi device\n");
		return -EINVAL;
	}

	flash = dev_get_uclass_priv(new);

	buf = memalign(ARCH_DMA_MINALIGN, CONFIG_USERID_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto err;
	}

	offset = CONFIG_USERID_OFFSET;
	size = flash->page_size;
	ret = spi_flash_read(flash, offset, size, buf);
	if (ret) {
		pr_err("read userid from offset 0x%lx failed.\n", offset);
		ret = -EIO;
		goto err;
	}

	memcpy(&head, buf, sizeof(head));
	if (head.magic != USERID_HEADER_MAGIC)
		goto err;

	offset += flash->page_size;
	size = CONFIG_USERID_SIZE - flash->page_size;
	ret = spi_flash_read(flash, offset, size, (buf + flash->page_size));
	if (ret) {
		pr_err("read userid from offset 0x%lx failed.\n", offset);
		ret = -EIO;
		goto err;
	}

	ret = userid_import(buf);

err:
	if (buf)
		free(buf);
	return ret;
}

#ifndef CONFIG_SPL_BUILD
static int spinor_userid_save(void)
{
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	struct udevice *new, *bus_dev;
	struct spi_flash *flash;
	int ret = -1;
	u32 len;
	u8 *buf;

	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (ret) {
		pr_err("Failed to find a spi device\n");
		return -EINVAL;
	}

	flash = dev_get_uclass_priv(new);

	buf = memalign(ARCH_DMA_MINALIGN, CONFIG_USERID_SIZE);
	if (!buf) {
		pr_err("Failed to malloc buffer.\n");
		ret = -ENOMEM;
		goto err;
	}
	memset(buf, 0xff, CONFIG_USERID_SIZE);
	ret = userid_export(buf);
	if (ret <= 0) {
		pr_err("Failed to export userid to buffer.\n");
		goto err;
	}
	len = ROUND(ret, flash->erase_size);

	pr_info("Erasing ...\n");
	if (spi_flash_erase(flash, CONFIG_USERID_OFFSET, len)) {
		pr_err("SPINOR: erase failed\n");
		ret = -1;
		goto err;
	}

	pr_info("Writing ...\n");
	ret = spi_flash_write(flash, CONFIG_USERID_OFFSET, len, buf);
	if (ret) {
		pr_err("SPINOR: save userid failed.\n");
		ret = -2;
		goto err;
	}

err:
	if (buf)
		free(buf);
	return 0;
}
#endif

U_BOOT_USERID_LOCATION(spinor) = {
	.name	= "SPINOR",
	.load	= spinor_userid_load,
#ifndef CONFIG_SPL_BUILD
	.save	= spinor_userid_save,
#endif
};
