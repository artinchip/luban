// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 *
 * Aneesh V <aneesh@ti.com>
 */

#include <common.h>
#include <dm.h>
#include <log.h>
#include <part.h>
#include <spl.h>
#include <linux/compiler.h>
#include <errno.h>
#include <asm/u-boot.h>
#include <errno.h>
#include <mmc.h>
#include <image.h>
#if CONFIG_SPL_FS_FAT_ARTINCHIP
#include <artinchip_spl_fat.h>
#endif

#ifdef CONFIG_AUTO_CALCULATE_PART_CONFIG
#include <generated/image_cfg_part_config.h>
#endif
#ifdef CONFIG_VIDEO_ARTINCHIP
#include <memalign.h>
#include <cpu_func.h>
#include <artinchip_ve.h>
#include <artinchip/artinchip_fb.h>
#endif

static int mmc_load_legacy(struct spl_image_info *spl_image, struct mmc *mmc,
			   ulong sector, struct image_header *header)
{
	u32 image_offset_sectors;
	u32 image_size_sectors;
	unsigned long count;
	u32 image_offset;
	int ret;

	ret = spl_parse_image_header(spl_image, header);
	if (ret)
		return ret;

	/* convert offset to sectors - round down */
	image_offset_sectors = spl_image->offset / mmc->read_bl_len;
	/* calculate remaining offset */
	image_offset = spl_image->offset % mmc->read_bl_len;

	/* convert size to sectors - round up */
	image_size_sectors = (spl_image->size + mmc->read_bl_len - 1) /
			     mmc->read_bl_len;

	/* Read the header too to avoid extra memcpy */
	count = blk_dread(mmc_get_blk_desc(mmc),
			  sector + image_offset_sectors,
			  image_size_sectors,
			  (void *)(ulong)spl_image->load_addr);
	debug("read %x sectors to %lx\n", image_size_sectors,
	      spl_image->load_addr);
	if (count != image_size_sectors)
		return -EIO;

	if (image_offset)
		memmove((void *)(ulong)spl_image->load_addr,
			(void *)(ulong)spl_image->load_addr + image_offset,
			spl_image->size);

	return 0;
}

static ulong h_spl_load_read(struct spl_load_info *load, ulong sector,
			     ulong count, void *buf)
{
	struct mmc *mmc = load->dev;

	return blk_dread(mmc_get_blk_desc(mmc), sector, count, buf);
}

static __maybe_unused unsigned long spl_mmc_raw_uboot_offset(int part)
{
#if IS_ENABLED(CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR)
	if (part == 0)
		return CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_DATA_PART_OFFSET;
#endif

	return 0;
}

static __maybe_unused
int mmc_load_image_raw_sector(struct spl_image_info *spl_image,
			      struct mmc *mmc, unsigned long sector)
{
	unsigned long count;
	struct image_header *header;
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	int ret = 0;

	header = spl_get_load_buffer(-sizeof(*header), bd->blksz);

	/* read image header to find the image size & load address */
	count = blk_dread(bd, sector, 1, header);
	debug("hdr read sector %lx, count=%lu\n", sector, count);
	if (count == 0) {
		ret = -EIO;
		goto end;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.dev = mmc;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = mmc->read_bl_len;
		load.read = h_spl_load_read;
		ret = spl_load_simple_fit(spl_image, &load, sector, header);
	} else if (IS_ENABLED(CONFIG_SPL_LOAD_IMX_CONTAINER)) {
		struct spl_load_info load;

		load.dev = mmc;
		load.priv = NULL;
		load.filename = NULL;
		load.bl_len = mmc->read_bl_len;
		load.read = h_spl_load_read;

		ret = spl_load_imx_container(spl_image, &load, sector);
	} else {
		ret = mmc_load_legacy(spl_image, mmc, sector, header);
	}

end:
	if (ret) {
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
		puts("mmc_load_image_raw_sector: mmc block read error\n");
#endif
		return -1;
	}

	return 0;
}

static int spl_mmc_get_device_index(u32 boot_device)
{
	switch (boot_device) {
	case BOOT_DEVICE_MMC1:
		return 0;
	case BOOT_DEVICE_MMC2:
	case BOOT_DEVICE_MMC2_2:
		return 1;
	}

#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
	printf("spl: unsupported mmc boot device.\n");
#endif

	return -ENODEV;
}

static int spl_mmc_find_device(struct mmc **mmcp, u32 boot_device)
{
	int err, mmc_dev;
	char num_dev = 0, cur_dev = 0;

	mmc_dev = cur_dev = spl_mmc_get_device_index(boot_device);
	if (cur_dev < 0)
		return cur_dev;

#if CONFIG_IS_ENABLED(DM_MMC)
	err = mmc_init_device(cur_dev);
	if (err) {
		num_dev = uclass_id_count(UCLASS_MMC);
		for (cur_dev = 0; err; cur_dev++) {
			if (cur_dev == mmc_dev)
				continue;
			err = mmc_init_device(cur_dev);
			if (!err)
				break;
			if (cur_dev >= num_dev)
				return err;
		}
	}
#else
	err = mmc_initialize(NULL);
#endif /* DM_MMC */
	if (err) {
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
		printf("spl: could not initialize mmc. error: %d\n", err);
#endif
		return err;
	}
	*mmcp = find_mmc_device(cur_dev);
	err = *mmcp ? 0 : -ENODEV;
	if (err) {
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
		printf("spl: could not find mmc device %d. error: %d\n",
		       cur_dev, err);
#endif
		return err;
	}

	return 0;
}

#ifdef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_PARTITION
static int mmc_load_image_raw_partition(struct spl_image_info *spl_image,
					struct mmc *mmc, int partition,
					unsigned long sector)
{
	struct disk_partition info;
	int err;

#ifdef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_PARTITION_TYPE
	int type_part;
	/* Only support MBR so DOS_ENTRY_NUMBERS */
	for (type_part = 1; type_part <= DOS_ENTRY_NUMBERS; type_part++) {
		err = part_get_info(mmc_get_blk_desc(mmc), type_part, &info);
		if (err)
			continue;
		if (info.sys_ind ==
			CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_PARTITION_TYPE) {
			partition = type_part;
			break;
		}
	}
#endif

	err = part_get_info(mmc_get_blk_desc(mmc), partition, &info);
	if (err) {
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
		puts("spl: partition error\n");
#endif
		return -1;
	}

#ifdef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
	return mmc_load_image_raw_sector(spl_image, mmc, info.start + sector);
#else
	return mmc_load_image_raw_sector(spl_image, mmc, info.start);
#endif
}
#endif

#ifdef CONFIG_SPL_OS_BOOT
#ifdef CONFIG_VIDEO_ARTINCHIP
#define CONFIG_LOGO_OFFSET	 (4096)
#define CONFIG_IMAGE_HEADER_SIZE (4 << 10)
static int mmc_load_logo(struct mmc *mmc)
{
	unsigned int header_len, first_block, other_block;
	const struct fdt_property *fdt_prop;
	unsigned char *fit, *dst;
	int i, offset, next_offset;
	int tag = FDT_PROP;
	fdt32_t image_len;
	struct udevice *dev;
	int ret = -EINVAL;
	u32 cnt;

	ret = uclass_first_device(UCLASS_VIDEO, &dev);
	if (ret) {
		pr_err("Failed to find display udevice\n");
		return ret;
	}

	fit = memalign(DECODE_ALIGN, CONFIG_IMAGE_HEADER_SIZE);
	if (!fit) {
		puts("Failed to malloc for fit image!\n");
		return -ENOMEM;
	}

	cnt = CONFIG_IMAGE_HEADER_SIZE / mmc->read_bl_len;
	ret = blk_dread(mmc_get_blk_desc(mmc), CONFIG_LOGO_OFFSET, cnt, fit);
	if (ret != cnt) {
		puts("Failed to read logo image from MMC/SD!\n");
		ret = -ENOMEM;
		goto out;
	}

	offset = fit_image_get_node(fit, "boot");
	if (offset < 0) {
		puts("Failed to find boot node in logo itb\n");
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

	dst = memalign(DECODE_ALIGN, ALIGN(image_len, CONFIG_IMAGE_HEADER_SIZE));
	if (!dst) {
		puts("Failed to malloc for boot logo image\n");
		ret = -ENOMEM;
		goto out;
	}

	if (image_len < CONFIG_IMAGE_HEADER_SIZE) {
		first_block = image_len;
		other_block = 0;
	} else {
		first_block = CONFIG_IMAGE_HEADER_SIZE - header_len;
		other_block = ALIGN(image_len - first_block,
				CONFIG_IMAGE_HEADER_SIZE);
	}
	memcpy(dst, fit + header_len, first_block);

	if (other_block) {
		cnt = other_block / mmc->read_bl_len;
		ret = blk_dread(mmc_get_blk_desc(mmc), CONFIG_LOGO_OFFSET +
				(CONFIG_IMAGE_HEADER_SIZE / MMC_MAX_BLOCK_LEN),
				cnt, dst + first_block);
		if (ret != cnt) {
			puts("Failed to read boot logo other block\n");
			goto out;
		}
	}
	flush_dcache_range((uintptr_t)dst, (uintptr_t)dst + image_len);

	if (dst[1] == 'P' || dst[2] == 'N' || dst[3] == 'G')
		aic_png_decode(dst, image_len);
	else if (dst[0] == 0xff || dst[1] == 0xd8)
		aic_jpeg_decode(dst, image_len);
	else
		puts("Invaild logo file format, need a png/jpeg image\n");

	aicfb_update_ui_layer(dev);
	aicfb_startup_panel(dev);
	return 0;

out:
	if (fit)
		free(fit);
	if (dst)
		free(dst);

	return ret;
}
#endif
static int mmc_load_image_raw_os(struct spl_image_info *spl_image,
				 struct mmc *mmc)
{
	int ret;

#if defined(CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTOR)
	unsigned long count;

	count = blk_dread(mmc_get_blk_desc(mmc),
		CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTOR,
		CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTORS,
		(void *) CONFIG_SYS_SPL_ARGS_ADDR);
	if (count != CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTORS) {
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
		puts("mmc_load_image_raw_os: mmc block read error\n");
#endif
		return -1;
	}
#endif	/* CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTOR */

	ret = mmc_load_image_raw_sector(spl_image, mmc,
		CONFIG_SYS_MMCSD_RAW_MODE_KERNEL_SECTOR);
	if (ret)
		return ret;

#ifdef CONFIG_VIDEO_ARTINCHIP
	mmc_load_logo(mmc);
#endif

	if (spl_image->os != IH_OS_LINUX && spl_image->os != IH_OS_TEE
	    && spl_image->os != IH_OS_OPENSBI) {
		puts("Expected image is not found. Trying to start U-boot\n");
		return -ENOENT;
	}

	return 0;
}
#else
int spl_start_uboot(void)
{
	return 1;
}
static int mmc_load_image_raw_os(struct spl_image_info *spl_image,
				 struct mmc *mmc)
{
	return -ENOSYS;
}
#endif

#ifdef CONFIG_SYS_MMCSD_FS_BOOT_PARTITION
static int spl_mmc_do_fs_boot(struct spl_image_info *spl_image, struct mmc *mmc,
			      const char *filename)
{
	int err = -ENOSYS;

#ifdef CONFIG_SPL_FS_FAT
	if (!spl_start_uboot()) {
		err = spl_load_image_fat_os(spl_image, mmc_get_blk_desc(mmc),
			CONFIG_SYS_MMCSD_FS_BOOT_PARTITION);
		if (!err)
			return err;
	}
#ifdef CONFIG_SPL_FS_LOAD_PAYLOAD_NAME
#ifdef CONFIG_SPL_FS_FAT_ARTINCHIP
	err = aic_spl_load_image_fat(spl_image, mmc_get_blk_desc(mmc),
				     filename);
	if (!err)
		return err;
#else
	err = spl_load_image_fat(spl_image, mmc_get_blk_desc(mmc),
				 CONFIG_SYS_MMCSD_FS_BOOT_PARTITION,
				 filename);
	if (!err)
		return err;
#endif
#endif
#endif
#ifdef CONFIG_SPL_FS_EXT4
	if (!spl_start_uboot()) {
		err = spl_load_image_ext_os(spl_image, mmc_get_blk_desc(mmc),
			CONFIG_SYS_MMCSD_FS_BOOT_PARTITION);
		if (!err)
			return err;
	}
#ifdef CONFIG_SPL_FS_LOAD_PAYLOAD_NAME
	err = spl_load_image_ext(spl_image, mmc_get_blk_desc(mmc),
				 CONFIG_SYS_MMCSD_FS_BOOT_PARTITION,
				 filename);
	if (!err) {
		pr_err("load image from mmc failed.\n");
		return err;
	}
#endif
#endif

#if defined(CONFIG_SPL_FS_FAT) || defined(CONFIG_SPL_FS_EXT4)
	err = -ENOENT;
#endif

	return err;
}
#else
static int spl_mmc_do_fs_boot(struct spl_image_info *spl_image, struct mmc *mmc,
			      const char *filename)
{
	return -ENOSYS;
}
#endif

u32 __weak spl_mmc_boot_mode(const u32 boot_device)
{
#if defined(CONFIG_SPL_FS_FAT) || defined(CONFIG_SPL_FS_EXT4)
	return MMCSD_MODE_FS;
#elif defined(CONFIG_SUPPORT_EMMC_BOOT)
	return MMCSD_MODE_EMMCBOOT;
#else
	return MMCSD_MODE_RAW;
#endif
}

#ifdef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_PARTITION
int __weak spl_mmc_boot_partition(const u32 boot_device)
{
	return CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_PARTITION;
}
#endif

unsigned long __weak spl_mmc_get_uboot_raw_sector(struct mmc *mmc,
						  unsigned long raw_sect)
{
	return raw_sect;
}

int default_spl_mmc_emmc_boot_partition(struct mmc *mmc)
{
	int part;
#ifdef CONFIG_SYS_MMCSD_RAW_MODE_EMMC_BOOT_PARTITION
	part = CONFIG_SYS_MMCSD_RAW_MODE_EMMC_BOOT_PARTITION;
#else
	/*
	 * We need to check what the partition is configured to.
	 * 1 and 2 match up to boot0 / boot1 and 7 is user data
	 * which is the first physical partition (0).
	 */
	part = (mmc->part_config >> 3) & PART_ACCESS_MASK;
	if (part == 7)
		part = 0;
#endif
	return part;
}

int __weak spl_mmc_emmc_boot_partition(struct mmc *mmc)
{
	return default_spl_mmc_emmc_boot_partition(mmc);
}

int spl_mmc_load(struct spl_image_info *spl_image,
		 struct spl_boot_device *bootdev,
		 const char *filename,
		 int raw_part,
		 unsigned long raw_sect)
{
	static struct mmc *mmc;
	u32 boot_mode;
	int err = 0;
	__maybe_unused int part = 0;

	/* Perform peripheral init only once */
	if (!mmc) {
		err = spl_mmc_find_device(&mmc, bootdev->boot_device);
		if (err)
			return err;

		err = mmc_init(mmc);
		if (err) {
			mmc = NULL;
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
			printf("spl: mmc init failed with error: %d\n", err);
#endif
			return err;
		}
	}

	boot_mode = spl_mmc_boot_mode(bootdev->boot_device);
	err = -EINVAL;
	switch (boot_mode) {
	case MMCSD_MODE_EMMCBOOT:
		part = spl_mmc_emmc_boot_partition(mmc);

		if (CONFIG_IS_ENABLED(MMC_TINY))
			err = mmc_switch_part(mmc, part);
		else
			err = blk_dselect_hwpart(mmc_get_blk_desc(mmc), part);

		if (err) {
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
			puts("spl: mmc partition switch failed\n");
#endif
			return err;
		}
		/* Fall through */
	case MMCSD_MODE_RAW:
		debug("spl: mmc boot mode: raw\n");

		if (!spl_start_uboot()) {
			err = mmc_load_image_raw_os(spl_image, mmc);
			/* Double check after linux image is loaded. */
			if (!spl_start_uboot() && !err)
				return err;
		}

		raw_sect = spl_mmc_get_uboot_raw_sector(mmc, raw_sect);

#ifdef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_PARTITION
		err = mmc_load_image_raw_partition(spl_image, mmc, raw_part,
						   raw_sect);
		if (!err)
			return err;
#endif
#ifdef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
		err = mmc_load_image_raw_sector(spl_image, mmc,
				raw_sect + spl_mmc_raw_uboot_offset(part));
		if (!err)
			return err;
#endif
		/* If RAW mode fails, try FS mode. */
	case MMCSD_MODE_FS:
		debug("spl: mmc boot mode: fs\n");

		err = spl_mmc_do_fs_boot(spl_image, mmc, filename);
		if (!err)
			return err;

		break;
#ifdef CONFIG_SPL_LIBCOMMON_SUPPORT
	default:
		puts("spl: mmc: wrong boot mode\n");
#endif
	}

	return err;
}

int spl_mmc_load_image(struct spl_image_info *spl_image,
		       struct spl_boot_device *bootdev)
{
	return spl_mmc_load(spl_image, bootdev,
#ifdef CONFIG_SPL_FS_LOAD_PAYLOAD_NAME
			    CONFIG_SPL_FS_LOAD_PAYLOAD_NAME,
#else
			    NULL,
#endif
#ifdef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_PARTITION
			    spl_mmc_boot_partition(bootdev->boot_device),
#else
			    0,
#endif
#ifdef CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR
			    CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR);
#else
			    0);
#endif
}

SPL_LOAD_IMAGE_METHOD("MMC1", 0, BOOT_DEVICE_MMC1, spl_mmc_load_image);
SPL_LOAD_IMAGE_METHOD("MMC2", 0, BOOT_DEVICE_MMC2, spl_mmc_load_image);
SPL_LOAD_IMAGE_METHOD("MMC2_2", 0, BOOT_DEVICE_MMC2_2, spl_mmc_load_image);
