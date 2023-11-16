/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */

#ifndef __D211_H__
#define __D211_H__
#include <linux/sizes.h>

/*
 * Skip lowlelvel init, for SPL, boot rom already init cp15, for U-Boot, SPL
 * already enable cache, cpu_init_cp15 will disable it again.
 */
#define CONFIG_SKIP_LOWLEVEL_INIT

/* Platform memory information */
#ifndef CONFIG_SEMIHOSTING
#define D211_SRAM_BASE			(0x00100000)
#define D211_SRAM_SIZE			(0x00018000)
#define D211_SDRAM_BASE			(0x40000000)
#else
/* mapping to dram area for fvp */
#define D211_SRAM_BASE			(0x84000000)
#define D211_SRAM_SIZE			(0x00018000)
#define D211_SDRAM_BASE			(0x80000000)
#endif

/*
 * Boot logo size
 * Uboot support png/jpg image logo, but spl just support png image
 */
#define LOGO_MAX_SIZE				(CONFIG_LOGO_IMAGE_SIZE << 10)

/* Miscellaneous configurable options */
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG
#define CONFIG_CMDLINE_TAG

#define CONFIG_SYS_HZ_CLOCK		4000000 /* Timer is clocked at 4MHz */
#define RISCV_SMODE_TIMER_FREQ          CONFIG_SYS_HZ_CLOCK
#define RISCV_MMODE_TIMER_FREQ          CONFIG_SYS_HZ_CLOCK
#define CONFIG_SYS_MAXARGS		32
#define CONFIG_SYS_BOOTM_LEN	SZ_64M
#define CONFIG_SYS_CBSIZE		SZ_1K
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16)

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN			SZ_16M

/* Allow to overwrite env for serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

/* SPL support */
#ifdef CONFIG_SPL

#define BROM_RAM_SIZE			0x3000
#define CONFIG_SPL_MAX_SIZE		(CONFIG_SPL_SIZE_LIMIT)
/* Set SPL initali stack to SRAM Top */
#define CONFIG_SPL_STACK		(D211_SRAM_BASE + D211_SRAM_SIZE)

#define CONFIG_SPL_BSS_START_ADDR	(CONFIG_SPL_TEXT_BASE + CONFIG_SPL_MAX_SIZE)
#define CONFIG_SPL_BSS_MAX_SIZE		0x00002000 /* 8 KiB */

/* SPL Falcon */
#define CONFIG_SYS_SPL_ARGS_ADDR		0x43F00000

#ifndef CONFIG_SPL_FS_LOAD_PAYLOAD_NAME
#define CONFIG_SPL_FS_LOAD_PAYLOAD_NAME	"bootcfg.txt"
#endif
#endif /* #ifdef CONFIG_SPL */

/* SPL -> Uboot */
#define CONFIG_SYS_UBOOT_START		CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_TEXT_BASE + SZ_2M - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_UBOOT_BASE		CONFIG_SYS_UBOOT_START

/*MMC SD*/
#define CONFIG_SYS_MMC_MAX_DEVICE		2
#define MMC_SUPPORTS_TUNING

/* NAND support */
#define CONFIG_SYS_MAX_NAND_DEVICE		1
#define CONFIG_SYS_NAND_U_BOOT_OFFS		(0x00000000)

/* SPI NOR Flash support */

/* UBoot -> Kernel */
#define CONFIG_LOADADDR					D211_SDRAM_BASE
#define CONFIG_SYS_LOAD_ADDR			CONFIG_LOADADDR

/* DRAM */
#define CONFIG_SYS_SDRAM_BASE			D211_SDRAM_BASE

/* Extra environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS		""

#define CONFIG_SYS_MMC_ENV_DEV			0
#define CONFIG_SYS_MTDPARTS_RUNTIME

/* USB */
#define CONFIG_SYS_USB_OHCI_MAX_ROOT_PORTS	1

#endif  /* __D211_H__ */
