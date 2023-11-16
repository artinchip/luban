// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ArtInChip Technology Co.,Ltd
 * Author: Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <dm/uclass.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <video.h>
#include <artinchip/artinchip_fb.h>
#include <artinchip_ve.h>
#include <mmc.h>
#include <env.h>
#include <spl.h>
#include <hang.h>
#include <init.h>
#include <spi.h>
#include <spi_flash.h>
#include <linux/io.h>
#include <debug_uart.h>
#include <fdt_support.h>
#include <asm/arch/boot_param.h>
#include <asm/arch/usb_detect.h>
#include <serial.h>
#include <linux/delay.h>
#include <userid.h>

#define usleep_range(a, b) udelay((b))

#ifdef CONFIG_SPL_SPI_NAND_TINY
#include <artinchip_spinand.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#define MAX_MTDIDS 64
#define MAX_MTDPARTS 256
#define LOGO_OFFSET	(0x120000)

#ifdef CONFIG_DEBUG_UART_BOARD_INIT

#define USE_UART1

#define GPIO_CMU_CFG_REG     ((void *)(0x1802083C))
#define CMU_PLL_INT1_CFG_REG ((void *)(0x18020004))

#ifdef USE_UART1
#define UART1_CMU_CFG_REG    ((void *)(0x18020844))
#define UART1_LSR_REG        ((void *)(0x18711014))
#define UART1_IER_REG        ((void *)(0x18711004))
#define GPIO_PA4_PIN_CFG_REG ((void *)0x18700090)
#define GPIO_PA5_PIN_CFG_REG ((void *)0x18700094)
#endif
#ifdef USE_UART0
#define UART0_CMU_CFG_REG    ((void *)(0x18020840))
#define UART0_LSR_REG        ((void *)(0x18710014))
#define UART0_IER_REG        ((void *)(0x18710004))

#define GPIO_PN0_PIN_CFG_REG ((void *)0x18700E80)
#define GPIO_PN1_PIN_CFG_REG ((void *)0x18700E84)
#endif

#define LSR_TX_EMP_BIT       BIT(6)


void board_debug_uart_init(void)
{
	u32 val = 0;

	/* Reset and Gating GPIO */
	writel(0x3100, GPIO_CMU_CFG_REG);
	writel(0x88153100, CMU_PLL_INT1_CFG_REG);

#ifdef USE_UART1
	writel(0x325, GPIO_PA4_PIN_CFG_REG);
	writel(0x325, GPIO_PA5_PIN_CFG_REG);

	val = readl(UART1_CMU_CFG_REG);
	if (val & 0x3100) {
		/* Wait for UART Tx FIFO to be empty, if UART already enabled */
		while ((readl(UART1_LSR_REG) & LSR_TX_EMP_BIT) == 0)
			;
		writel(0, UART1_CMU_CFG_REG);
	};
	/* Reset and Gating UART */
	writel(0x3118, UART1_CMU_CFG_REG);
#endif

#ifdef USE_UART0
	writel(0x324, GPIO_PN0_PIN_CFG_REG);
	writel(0x324, GPIO_PN1_PIN_CFG_REG);

	val = readl(UART0_CMU_CFG_REG);
	if (val & 0x3100) {
		/* Wait for UART Tx FIFO to be empty, if UART already enabled */
		while ((readl(UART0_LSR_REG) & LSR_TX_EMP_BIT) == 0)
			;
		writel(0, UART0_CMU_CFG_REG);
	};
	/* Reset and Gating UART */
	writel(0x3118, UART0_CMU_CFG_REG);
#endif
}
#endif

#if defined(CONFIG_SPL_BUILD)
void board_init_f(ulong dummy)
{
	int ret;

#ifdef CONFIG_ARTINCHIP_DEBUG_BOOT_TIME
	u32 *p = (u32 *)BOOT_TIME_SPL_START;
	/* SPL start time */
	*p = aic_timer_get_us();
#endif

#ifdef CONFIG_DEBUG_UART
	/*
	 * For SPL, Use DEBUG UART to enable serial output,
	 * don't use dm serial driver, because it spend too much time to
	 * initialize driver.
	 */
	log_notice("\nspl:debug uart enabled in %s\n", __func__);
#endif
	ret = spl_early_init();
	if (ret)
		panic("spl_early_init() failed: %d\n", ret);

	arch_cpu_init_dm();

	preloader_console_init();

	ret = spl_board_init_f();
	if (ret)
		panic("spl_board_init_f() failed: %d\n", ret);
}

void spl_board_init(void)
{
	enable_caches();
}

void spl_board_prepare_for_linux(void)
{
	usb_dev_connection_check_end(0);
}
#endif

int board_init(void)
{
	return 0;
}

static void setup_boot_device(void)
{
	enum boot_device bd;

	/*
	 * Boot ROM detects Boot device and pass it to SPL,
	 * SPL pass it to U-Boot.
	 */
	bd = aic_get_boot_device();
	switch (bd) {
	case BD_SDMC0:
		env_set("boot_device", "mmc");
		env_set("boot_devnum", "0");
		debug("Booting from eMMC...\n");
		break;
	case BD_SDMC1:
		env_set("boot_device", "mmc");
		env_set("boot_devnum", "1");
		debug("Booting from SD Card...\n");
		break;
	case BD_SDFAT32:
		env_set("boot_device", "fat");
		env_set("boot_devnum", "1");
		debug("Booting from SD Card FATFS...\n");
		break;
	case BD_SPINOR:
		env_set("boot_device", "nor");
		env_set("boot_devnum", "0");
		debug("Booting from SPI NOR...\n");
		break;
	case BD_SPINAND:
		env_set("boot_device", "nand");
		env_set("boot_devnum", "0");
		debug("Booting from SPI NAND...\n");
		break;
	case BD_USB:
		env_set("boot_device", "usb");
		env_set("boot_devnum", "0");
		debug("Booting from USBUPG...\n");
		break;
	default:
		pr_err("Unknown boot device id %d...\n", (int)bd);
		break;
	}
}

#if CONFIG_IS_ENABLED(OF_CONTROL) && defined(CONFIG_FIT_SIGNATURE)
static int set_dev_part(const void *blob, char *propname)
{
	struct disk_partition info;
	struct blk_desc *desc;
	char *media, *devnum, dev_name[16];
	const char *part_name;
	int part, ret;

	media = env_get("boot_device");
	devnum = env_get("boot_devnum");
	/* get part name */
	part_name = fdtdec_get_chosen_prop(blob, propname);
	if (part_name) {
		/* get dev desc  */
		ret = blk_get_device_by_str(media, devnum, &desc);
		if (ret < 0)
			return ret;

		/* get part number  */
		part = part_get_info_by_name(desc, part_name, &info);
		if (part < 0)
			return -1;
		snprintf(dev_name, sizeof(dev_name), "/dev/mmcblk%sp%d",
					devnum, part);

		env_set(propname, dev_name);
	}

	return 0;
}

static int setup_dm_verity_part(void)
{
	int ret;

	ret = set_dev_part(gd->fdt_blob, "root_part");
	if (ret < 0)
		return ret;

	ret = set_dev_part(gd->fdt_blob, "hash_part");
	if (ret < 0)
		return ret;

	return 0;
}
#endif

#ifdef CONFIG_VIDEO_ARTINCHIP
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

static int mmc_load_logo(struct udevice *dev, int id)
{
#ifdef CONFIG_MMC
	struct mmc *mmc = find_mmc_device(id);
	struct disk_partition part_info;
	unsigned char *dst;
	int ret;
	u32 cnt;

	ret = part_get_info_by_name(mmc_get_blk_desc(mmc), "logo", &part_info);
	if (ret < 0) {
		pr_err("Get logo partition information failed.\n");
		return -1;
	}

	dst = memalign(DECODE_ALIGN, LOGO_MAX_SIZE);
	if (!dst) {
		pr_err("Failed to malloc for logo image!\n");
		return -ENOMEM;
	}

	cnt = LOGO_MAX_SIZE / mmc->read_bl_len;
	ret = blk_dread(mmc_get_blk_desc(mmc), part_info.start, cnt, dst);
	if (ret != cnt) {
		free(dst);
		pr_err("Failed to read logo image from MMC/SD!\n");
		return -EIO;
	}

	aic_logo_decode(dst, LOGO_MAX_SIZE);
	free(dst);
#endif
	return 0;
}

static int spinor_load_logo(struct udevice *dev)
{
#ifdef CONFIG_DM_SPI_FLASH
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	struct udevice *new, *bus_dev;
	struct spi_flash *flash;
	unsigned char *dst;
	int ret;

	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (ret) {
		pr_err("Failed to find a spi device\n");
		return -EINVAL;
	}
	flash = dev_get_uclass_priv(new);

	dst = memalign(DECODE_ALIGN, LOGO_MAX_SIZE);
	if (!dst) {
		pr_err("Failed to malloc for logo image!\n");
		return -ENOMEM;
	}

	ret = spi_flash_read(flash, LOGO_OFFSET, LOGO_MAX_SIZE, dst);
	if (ret) {
		pr_err("Failed to read logo image for SPINOR\n");
		free(dst);
		return ret;
	}

	aic_logo_decode(dst, LOGO_MAX_SIZE);
	free(dst);
#endif
	return 0;
}

static int board_prepare_logo(struct udevice *dev)
{
	enum boot_device bd;
	int ret = -EINVAL;

	bd = aic_get_boot_device();
	switch (bd) {
	case BD_SDMC0:
		ret = mmc_load_logo(dev, 0);
		break;
	case BD_SDMC1:
		ret = mmc_load_logo(dev, 1);
		break;
	case BD_SDFAT32:
		ret = aic_disp_logo("sdburn", bd);
		break;
	case BD_SPINAND:
		ret = aic_disp_logo("boot", bd);
		break;
	case BD_SPINOR:
		ret = spinor_load_logo(dev);
		break;
	case BD_BOOTROM:
		ret = aic_disp_logo("usbburn", bd);
		break;
	default:
		pr_err("Unknown boot device id %d...\n", (int)bd);
		return ret;
	}

	return ret;
}

static int board_show_logo(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_first_device(UCLASS_VIDEO, &dev);
	if (ret) {
		pr_err("Failed to find aicfb udevice\n");
		return ret;
	}

	ret = board_prepare_logo(dev);
	if (!ret) {
		aicfb_update_ui_layer(dev);
		aicfb_startup_panel(dev);
	}
	return ret;
}
#endif /* CONFIG_VIDEO_ARTINCHIP  */

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, struct bd_info *bd)
{
#ifdef CONFIG_VIDEO_ARTINCHIP
	struct fdt_memory logo;
	struct video_priv *priv;
	struct udevice *dev;
	int ret, err;

	ret = uclass_find_first_device(UCLASS_VIDEO, &dev);
	if (ret) {
		pr_err("%s: failed to find aicfb udevice\n", __func__);
		return ret;
	}
	priv = dev_get_uclass_priv(dev);

	if (IS_ERR_OR_NULL(priv)) {
		pr_warn("%s: failed to find aic-logo info\n", __func__);
		return 0;
	}

	logo.start = (phys_addr_t)priv->fb;
	logo.end = (phys_addr_t)priv->fb + priv->fb_size - 1;

	err = fdtdec_add_reserved_memory(blob, "aic-logo", &logo, NULL, false);
	if (err < 0 && err != -FDT_ERR_EXISTS) {
		pr_err("%s: failed to add reserved memory\n", __func__);
		return err;
	}
#endif
	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */

int board_late_init(void)
{
	setup_boot_device();
#if defined(CONFIG_FIT_SIGNATURE)
	setup_dm_verity_part();
#endif
#ifdef CONFIG_VIDEO_ARTINCHIP
	board_show_logo();
#endif
#ifdef CONFIG_USERID_SUPPORT
	enum boot_device bd;

	bd = aic_get_boot_device();
	if (bd != BD_USB && bd != BD_SDFAT32)
		userid_init();
#endif

	return 0;
}

void board_mtdparts_default(const char **mtdids, const char **mtdparts)
{
	static char ids[MAX_MTDIDS];
	static char parts[MAX_MTDPARTS];
	struct mtd_info *mtd = get_mtd_device(NULL, 0);
	struct udevice *dev;
	int pos, ret, cnt;
	char *p;

	if (IS_ERR_OR_NULL(mtd)) {
		ret = uclass_first_device(UCLASS_MTD, &dev);
		if (ret && !dev) {
			pr_err("Find MTD device failed.\n");
			goto out;
		}

		device_probe(dev);
		mtd = get_mtd_device(NULL, 0);
	}
	if (IS_ERR_OR_NULL(mtd)) {
		pr_err("There is no mtd device.\n");
		goto out;
	}

	if ((gd->flags & GD_FLG_ENV_READY) == 0) {
		pr_warn("env is not loaded.\n");
		goto out;
	}

	p = env_get("MTD");
	if (!p) {
		pr_warn("Get MTD partition table from env failed.\n");
		goto out;
	}

	memset(ids, 0, MAX_MTDIDS);
	snprintf(ids, MAX_MTDIDS, "%s=", mtd->name);
	for (pos = 0; pos < strlen(p); pos++) {
		if (p[pos] == ':')
			break;
	}

	if (pos == strlen(p)) {
		pr_err("There is no mtd ids in partition table.\n");
		return;
	}

	if ((pos + strlen(ids)) >= MAX_MTDIDS) {
		pr_err("mtd ids is too long.\n");
		return;
	}

	if (strlen(p) >= MAX_MTDPARTS) {
		pr_err("mtd partition table is too long\n");
		return;
	}

	memcpy(ids + strlen(ids), p, pos);
	strcpy(parts, p);

	/* Check if it is set minimal boot mtdparts number */
	p = env_get("nand_boot_mtdparts_cnt");
	if (p) {
		cnt = simple_strtoul(p, NULL, 10);
		if (!cnt)
			goto all;
		/* Filter minimal parts */
		p = parts;
		while (*p != '\0') {
			p++;
			if (*p == ',')
				cnt--;
			if (cnt == 0)
				break;
		}

		/* OK, drop not relative mtdparts for u-boot */
		if (!cnt && *p == ',')
			*p = '\0';
	}
all:
	*mtdids = ids;
	*mtdparts = parts;

	pr_info("U-Boot stage mtdparts: %s\n", parts);
	return;
out:
	/* ENV is not ready, use default configuratioin if it is provided.*/
#ifdef CONFIG_MTDIDS_DEFAULT
	*mtdids = CONFIG_MTDIDS_DEFAULT;
#endif
#ifdef CONFIG_MTDPARTS_DEFAULT
	*mtdparts = CONFIG_MTDPARTS_DEFAULT;
#endif
	return;
}

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	gd->bd->bi_dram[0].size = gd->ram_size;
	return 0;
}

#ifdef CONFIG_SPL_OS_BOOT
static int search_boot_os(u8 *env_ram, int len)
{
	int i = 0, cnt = 0;

	while (i < len) {
		if (env_ram[i] != 0)
			cnt = 0;
		if ((env_ram[i] == 'b') &&
		    !memcmp(&env_ram[i], "boot_os=yes", 11))
			return 1;
		else if (env_ram[i] == 0)
			cnt++;
		if (cnt >= 2)
			break;
		i++;
	}

	return 0;
}

#ifdef CONFIG_SPL_MMC_SUPPORT
static int mmc_load_env_simple(int dev, u8 *env_ram)
{
	static struct mmc *mmc;
	unsigned long count, sector_start, sector_cnt;
	int err = -1;

	mmc = find_mmc_device(dev);
	if (!mmc)
		return err;

	sector_start = CONFIG_ENV_OFFSET / mmc->read_bl_len;
	sector_cnt = CONFIG_ENV_SIZE / mmc->read_bl_len;

	count = blk_dread(mmc_get_blk_desc(mmc), sector_start, sector_cnt,
			  env_ram);
	if (count != sector_cnt)
		return -1;

	return CONFIG_ENV_SIZE;
}
#endif

#ifdef CONFIG_SPL_SPI_NAND_TINY
static int spinand_load_env_simple(u8 *env_ram)
{
	struct spinand_device *spinand;
	unsigned long offset, remain;
	size_t rdlen;

	spinand = spl_spinand_init();
	if (IS_ERR_OR_NULL(spinand)) {
		pr_err("Tiny SPI NAND init failed. ret = %ld\n",
			PTR_ERR(spinand));
		return -1;
	}

	offset = CONFIG_ENV_OFFSET;
	remain = (uint32_t)CONFIG_ENV_SIZE;

#ifndef CONFIG_SYS_REDUNDAND_ENVIRONMENT
	spl_spi_nand_read(spinand, offset, remain, &rdlen, (void *)env_ram);
	if (remain != rdlen) {
		pr_err("Tiny SPI NAND read failed.\n");
		return -1;
	}

	return CONFIG_ENV_SIZE;
#else
	int read1_fail = 0, read2_fail = 0;
	u8 *buf = env_ram;
	u8 *buf_redund = (u8 *)CONFIG_ENV_RAM_ADDR + CONFIG_ENV_SIZE;
	char tmp_env1_flags, tmp_env2_flags;

	read1_fail = spl_spi_nand_read(spinand, CONFIG_ENV_OFFSET, remain,
				       &rdlen, (void *)buf);
	read2_fail = spl_spi_nand_read(spinand, CONFIG_ENV_OFFSET_REDUND,
				       remain, &rdlen, (void *)buf_redund);

	if (read1_fail && read2_fail) {
		pr_err("Tiny SPI NAND read failed.\n");
		return -1;
	}

	tmp_env1_flags = buf[4];
	tmp_env2_flags = buf_redund[4];

	if (tmp_env1_flags == 255 && tmp_env2_flags == 0)
		memcpy(env_ram, buf_redund, CONFIG_ENV_SIZE);
	else if (tmp_env2_flags > tmp_env1_flags)
		memcpy(env_ram, buf_redund, CONFIG_ENV_SIZE);

	return CONFIG_ENV_SIZE;
#endif
}
#endif

#ifdef CONFIG_SPL_SPI_LOAD
static int spinor_load_env_simple(u8 *env_ram)
{
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	struct udevice *new, *bus_dev;
	struct spi_flash *flash;
	int ret;

	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (ret) {
		pr_err("Failed to find a spi device\n");
		return -EINVAL;
	}

	flash = dev_get_uclass_priv(new);
	ret = spi_flash_read(flash, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE,
			     (void *)env_ram);
	if (ret) {
		pr_err("Failed to read env from SPINOR.\n");
		return -1;
	}
	return CONFIG_ENV_SIZE;
}
#endif
static int env_boot_os_flag(enum boot_device bd)
{
	u8 *env_ram = (u8 *)CONFIG_ENV_RAM_ADDR;
	int size = 0;

	switch (bd) {
#ifdef CONFIG_SPL_MMC_SUPPORT
	case BD_SDMC0:
		size = mmc_load_env_simple(0, env_ram);
		break;
	case BD_SDMC1:
		size = mmc_load_env_simple(1, env_ram);
		break;
#endif
	case BD_SPINAND:
#ifdef CONFIG_SPL_SPI_NAND_TINY
		size = spinand_load_env_simple(env_ram);
#endif
		break;
	case BD_SPINOR:
#ifdef CONFIG_SPL_SPI_LOAD
		size = spinor_load_env_simple(env_ram);
#endif
		break;
	default:
		break;
	}

	if (size > 0)
		return search_boot_os(env_ram, size);
	return 0;
}

#ifdef CONFIG_USERID_SUPPORT
static int userid_lock_flag(enum boot_device bd)
{
	int ret = 0;
	u32 flag = 0;

	ret = userid_init();
	if (ret) {
		return 0;
	}
	ret = userid_read("lock", 0, (void *)&flag, 4);
	if (ret <= 0)
		return 0;

	return (flag != 0);
}
#endif

#define START_UNKNOWN -1
#define START_KERNEL   0
#define START_UBOOT    1

extern int aic_upg_mode_detect(void);
/*
 * This API will be called multi-times
 */
int spl_start_uboot(void)
{
#ifndef CONFIG_SPL_OS_BOOT
	/* Falcon mode is not enabled, run u-boot directly */
	return START_UBOOT;
#else
	static int start = START_UNKNOWN;
	static int usb_dev_check;
	enum boot_device bd;
	int userid_lock = 0;

	if (start == START_UBOOT)
		return start;

	/* UNKNOWN, START_KERNEL: still need to check */

	bd = aic_get_boot_device();
	if (bd == BD_USB || bd == BD_SDFAT32) {
		/* If Boot for image upgrading, force to start U-Boot. */
		start = START_UBOOT;
		puts("Run U-Boot: upgrading mode\n");
		goto out;
	}

	/* break into full u-boot with CTRL + c/C */
	if (serial_tstc() && serial_getc() == CTRL('C')) {
		start = START_UBOOT;
		puts("Run U-Boot: got CTRL+C\n");
		goto out;
	}
	if (start == START_UNKNOWN) {
#ifdef CONFIG_CMD_AICUPG
		if (aic_upg_mode_detect()) {
			start = START_UBOOT;
			puts("Run U-Boot: UPG PIN is pressed or Software reset to enter UPG mode\n");
			goto out;
		}
#endif
		if (env_boot_os_flag(bd) != 1) {
			start = START_UBOOT;
			puts("Run U-Boot: boot_os=no in ENV, don't boot os\n");
			goto out;
		}
#ifdef CONFIG_UPDATE_UDISK_FATFS_ARTINCHIP
		/* Check udisk upgrading:
		 * Only check when userid is locked
		 */
		if (usb_host_udisk_connection_check()) {
			/* Device is connecting to UDISK, goto uboot to check
			 * whether it is going to perform UDISK upgrading
			 */
			start = START_UBOOT;
			puts("Run U-Boot: try UDISK upgrading\n");
			goto out;
		}
#endif
		usb_dev_check = 0;
#ifdef CONFIG_USERID_SUPPORT
		userid_lock = userid_lock_flag(bd);
#else
		userid_lock = 1;
#endif
		/*
		 * When userid partition is locked, it is no needed to check
		 * usb connection for userid
		 */
		if (!userid_lock)
			usb_dev_check++;
#ifdef CONFIG_AICUPG_FORCE_USBUPG_SUPPORT
		usb_dev_check++;
#endif

		/*
		 * Switch to Device mode, and check whether USB connecting to PC.
		 * This function should be called once.
		 */
		if (usb_dev_check)
			usb_dev_connection_check_start(0);
	}

	/*
	 * Check usb connecton status every time spl_start_uboot is called.
	 */
	if (usb_dev_check && usb_dev_connection_check_status(0)) {
		start = START_UBOOT;
		usb_dev_connection_check_end(0);
		puts("Run U-Boot: Device is connecting to PC with USB, try to burn userid\n");
		goto out;
	}
	start = START_KERNEL;
out:
	return start;
#endif
}
#endif

ulong board_spl_fit_size_align(ulong size)
{
	size = ALIGN(size, 0x20);

	return size;
}

/* Get the top of usable RAM */
ulong board_get_usable_ram_top(ulong total_size)
{
	if (gd->ram_top ==
	    CONFIG_SPL_OPENSBI_LOAD_ADDR + CONFIG_SPL_OPENSBI_SIZE)
		return gd->ram_top - CONFIG_SPL_OPENSBI_SIZE;
	else
		return gd->ram_top;
}
