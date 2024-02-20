// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <malloc.h>
#include <g_dnl.h>
#include <usb.h>
#include <artinchip/aicupg.h>
#include <artinchip/artinchip_fb.h>
#include <asm/arch/boot_param.h>
#include <config_parse.h>
#include <dm/uclass.h>
#include <env.h>

#if 0
#undef debug
#define debug printf
#endif

#define AICUPG_ARGS_MAX 4
#define WAIT_UPG_MODE_TMO_US 2000000

#if defined(CONFIG_MMC) || defined(CONFIG_SPL_MMC)
static int curr_device = -1;

static int image_header_check(struct image_header_pack *header)
{
	/*check header*/
	if ((strcmp(header->hdr.magic, "AIC.FW") != 0)) {
		pr_err("Error:image check failed,maybe not have a image in media!\n");
		return -1;
	}
	return 0;
}
#endif

__weak void do_brom_upg(void)
{
	printf("%s is not implemented.\n", __func__);
}

static int check_upg_mode(long start_tm, long tmo)
{
	long cur_tm, tm;
	int mode;

	cur_tm = timer_get_us();
	tm = (cur_tm - start_tm);

	if (tm < tmo)
		return 0;

	mode = aicupg_get_upg_mode();
	if (mode == UPG_MODE_BURN_USER_ID)
		return 0;
	if (mode == UPG_MODE_BURN_IMG_FORCE)
		return 0;

	return 1;
}

static int do_usb_protocol_upg(int intf)
{
	int ret, ck_mode;
	long start_tm;
	char *p;
	struct upg_init init;

	start_tm = timer_get_us();

	ck_mode = 0;
	init.mode = INIT_MODE(UPG_MODE_FULL_DISK_UPGRADE);
	p = env_get("upg_mode");
	if (p) {
		if (!strcmp(p, "userid")) {
			ck_mode = 1;
			init.mode = INIT_MODE(UPG_MODE_BURN_USER_ID);
#ifdef CONFIG_AICUPG_FORCE_USBUPG_SUPPORT
			/* Enter burn USERID mode also support force burn image */
			init.mode |= INIT_MODE(UPG_MODE_BURN_IMG_FORCE);
#endif
		} else if (!strcmp(p, "force")) {
			ck_mode = 1;
			init.mode = INIT_MODE(UPG_MODE_BURN_IMG_FORCE);
		}
		/* Remove this information after used, avoid to be saved
		 * to env partition
		 */
		env_set("upg_mode", "");
	}

	aicupg_initialize(&init);
	ret = usb_gadget_initialize(intf);
	if (ret) {
		printf("USB init failed: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	g_dnl_clear_detach();
	ret = g_dnl_register("usb_dnl_aicupg");
	if (ret)
		return CMD_RET_FAILURE;

	while (1) {
		if (g_dnl_detach())
			break;
		if (ctrlc())
			break;
		if (ck_mode && check_upg_mode(start_tm, WAIT_UPG_MODE_TMO_US)) {
			/* Host tool not set the mode, exit usb loop
			 * and boot kernel
			 */
			ret = CMD_RET_FAILURE;
			goto exit;
		}
		usb_gadget_handle_interrupts(intf);
	}
	ret = CMD_RET_SUCCESS;

exit:
	g_dnl_unregister();
	g_dnl_clear_detach();
	usb_gadget_release(intf);

	return ret;
}

#if defined(CONFIG_MMC) || defined(CONFIG_SPL_MMC)
static struct mmc *init_mmc_device(int dev, bool force_init)
{
	struct mmc *mmc;

	mmc = find_mmc_device(dev);
	if (!mmc) {
		pr_err("no mmc device at slot %x\n", dev);
		return NULL;
	}
	if (!mmc_getcd(mmc))
		force_init = true;

	if (force_init)
		mmc->has_init = 0;
	if (mmc_init(mmc))
		return NULL;
#ifdef CONFIG_BLOCK_CACHE
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	blkcache_invalidate(bd->if_type, bd->devnum);
#endif
	return mmc;
}
#endif

static int do_sdcard_upg(int intf)
{
	s32 ret = 0;
#if defined(CONFIG_MMC) || defined(CONFIG_SPL_MMC)
	struct image_header_pack *hdrpack;
	struct mmc *mmc;
	char *mmc_type;
	u32 cnt, n;
	struct disk_partition part_info;

	hdrpack = NULL;
	/*init MMC */
	if (curr_device < 0) {
		if (get_mmc_num() > 0) {
			curr_device = 0;
		} else {
			pr_err("No MMC device available\n");
			goto err;
		}
	}

	mmc = init_mmc_device(curr_device, false);
	if (!mmc) {
		pr_err("Init mmc device failed!\n");
		goto err;
	}

	/*SD card upgrade should select SD*/
	mmc_type = IS_SD(mmc) ? "SD" : "eMMC";
	if (!strcmp(mmc_type, "eMMC")) {
		if (get_mmc_num() > 1) {
			curr_device = 1;
			mmc = init_mmc_device(curr_device, false);
			if (!mmc) {
				pr_err("Init mmc device failed!\n");
				goto err;
			}
		} else {
			pr_err("No SD card is insert!..\n");
			goto err;
		}
	}

	/*Set GPT partition*/
	ret = aicupg_mmc_create_gpt_part(curr_device, true);
	if (ret < 0) {
		pr_err("Create GPT partitions failed\n");
		goto err;
	}

	/*load header*/
	ret = part_get_info_by_name(mmc_get_blk_desc(mmc), "image",
				    &part_info);
	if (ret == -1) {
		pr_err("Get partition information failed.\n");
		goto err;
	}
	hdrpack = (struct image_header_pack *)malloc(sizeof(struct image_header_pack));
	if (!hdrpack) {
		pr_err("Error, malloc buf failed.\n");
		goto err;
	}
	memset((struct image_header_pack *)hdrpack, 0, sizeof(struct image_header_pack));

	cnt = sizeof(struct image_header_pack) / mmc->read_bl_len;
	n = blk_dread(mmc_get_blk_desc(mmc), part_info.start, cnt, (void *)hdrpack);
	if (n != cnt) {
		pr_err("load header failed!\n");
		goto err;
	}

	/*checkout header*/
	ret = image_header_check(hdrpack);
	if (ret) {
		pr_err("check image header failed!\n");
		goto err;
	}

	/*when upgrade emmc,device should different*/
	hdrpack->hdr.media_dev_id = curr_device ? 0 : 1;
	/*write data to media*/
	ret = aicupg_sd_write(&hdrpack->hdr, mmc, part_info);
	if (ret == 0) {
		pr_err("sd card write data failed!\n");
		goto err;
	}

	free(hdrpack);
	ret = CMD_RET_SUCCESS;
	return ret;
err:
	if (hdrpack)
		free(hdrpack);
	ret = CMD_RET_FAILURE;
#endif
	return ret;
}

static int do_fat_upg(int intf, char *const blktype)
{
	int ret = 0;
#if defined(CONFIG_FS_FAT) || defined(CONFIG_SPL_FS_FAT)
	struct image_header_pack *hdrpack;
	struct mmc *mmc;
	loff_t actread;
	char num_dev = 0, cur_dev = 0;
	char *file_buf;
	char image_name[IMG_NAME_MAX_SIZ] = {0};
	char protection[PROTECTION_PARTITION_LEN] = {0};

	mmc = NULL;
	hdrpack = (struct image_header_pack *)malloc(sizeof(struct image_header_pack));
	if (!hdrpack) {
		pr_err("Error, malloc hdrpack failed.\n");
		return CMD_RET_FAILURE;
	}
	memset((struct image_header_pack *)hdrpack, 0,
		sizeof(struct image_header_pack));

	file_buf = (char *)malloc(1024);
	if (!file_buf) {
		pr_err("Error, malloc buf failed.\n");
		goto err;
	}
	memset((void *)file_buf, 0, 1024);

	if (!strcmp(blktype, "mmc")) {
		/*init MMC */
		if (curr_device < 0) {
			if (get_mmc_num() > 0) {
				curr_device = 1;
			} else {
				pr_err("No MMC device available\n");
				goto err;
			}
		}
		mmc = init_mmc_device(curr_device, false);
		if (!mmc) {
			num_dev = uclass_id_count(UCLASS_MMC);
			for (cur_dev = 0; !mmc; cur_dev++) {
				if (cur_dev == curr_device)
					continue;
				mmc = init_mmc_device(cur_dev, false);
				if (mmc)
					break;
				if (cur_dev >= num_dev) {
					pr_err("Init mmc device failed!\n");
					goto err;
				}
			}
			curr_device = cur_dev;
		}

		printf("curr_device:%d\n", curr_device);
		if (curr_device == 0)
			ret = fs_set_blk_dev("mmc", "0", FS_TYPE_FAT);
		else
			ret = fs_set_blk_dev("mmc", "1", FS_TYPE_FAT);
		if (ret != 0) {
			pr_err("Set blk dev failed!\n");
			goto err;
		}
#ifdef CONFIG_UPDATE_UDISK_FATFS_ARTINCHIP
	} else if (!strcmp(blktype, "udisk")) {
		/*usb init*/
		if (usb_init() < 0) {
			pr_err("usb init failed!\n");
			goto err;
		}

		/* try to recognize storage devices immediately */
		ret = usb_stor_scan(1);
		if (ret < 0) {
			pr_err("No udisk is insert!\n");
			goto err;
		}

		ret = fs_set_blk_dev("usb", "0", FS_TYPE_FAT);
		if (ret != 0) {
			pr_err("Set blk dev failed!\n");
			goto err;
		}

#ifdef CONFIG_VIDEO_ARTINCHIP
		ret = aic_disp_logo("udiskburn", BD_SDFAT32);
		if (ret)
			pr_err("Display udisk burn logo failed!\n");;
#endif

#endif
	} else {
		goto err;
	}

	/*load header*/
	ret = fat_read_file("bootcfg.txt", (void *)file_buf, 0, 1024, &actread);
	if (actread == 0 || ret != 0) {
		printf("Error:read file bootcfg.txt failed!\n");
		goto err;
	}

	ret = boot_cfg_get_image(file_buf, actread, image_name,
				 IMG_NAME_MAX_SIZ);
	if (ret < 0) {
		pr_err("get bootcfg.txt image name failed!\n");
		goto err;
	}

	ret = boot_cfg_get_protection(file_buf, actread, protection,
				      PROTECTION_PARTITION_LEN);
	if (ret < 0)
		pr_warn("No protected partition.\n");
	else
		pr_info("Protected=%s\n", protection);

	ret = fat_read_file(image_name, (void *)hdrpack, 0,
			   sizeof(struct image_header_pack), &actread);
	if (actread != sizeof(struct image_header_pack) || ret != 0) {
		printf("Error:read file %s failed!\n", image_name);
		goto err;
	}

	/*check header*/
	ret = image_header_check(hdrpack);
	if (ret) {
		pr_err("check image header failed!\n");
		goto err;
	}

	/*write data to media*/
	ret = aicupg_fat_write(image_name, protection, &hdrpack->hdr);
	if (ret == 0) {
		pr_err("fat write data failed!\n");
		goto err;
	}
#ifdef CONFIG_VIDEO_ARTINCHIP
	ret = aic_disp_logo("burn_done", BD_SDFAT32);
	if (ret)
		pr_err("display burn done logo failed\n");
#endif

	free(hdrpack);
	free(file_buf);
	ret = CMD_RET_SUCCESS;
	return ret;
err:
	if (hdrpack)
		free(hdrpack);
	if (file_buf)
		free(file_buf);
	ret = CMD_RET_FAILURE;
#endif
	return ret;
}

static int do_aicupg(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	char *devtype = NULL;
	int intf, ret = CMD_RET_USAGE;

	if ((argc == 1) || ((argc == 2) && !strcmp(argv[1], "brom"))) {
		do_brom_upg();
		return 0;
	}
	if ((argc < 3) || (argc > AICUPG_ARGS_MAX))
		return ret;

	devtype = argv[1]; /* mmc  usb fat */
	if (argc >= 4 && argv[3])
		intf = simple_strtoul(argv[3], NULL, 0);
	else
		intf = simple_strtoul(argv[2], NULL, 0);

	if (devtype == NULL)
		return ret;
	if (!strcmp(devtype, "usb"))
		ret = do_usb_protocol_upg(intf);
	if (!strcmp(devtype, "mmc"))
		ret = do_sdcard_upg(intf);
	if (!strcmp(devtype, "fat"))
		ret = do_fat_upg(intf, argv[2]);

	return ret;
}

U_BOOT_CMD(aicupg, AICUPG_ARGS_MAX, 0, do_aicupg,
	"ArtInChip firmware upgrade",
	"[devtype] [interface]\n"
	"  - devtype: should be usb, mmc, fat, brom\n"
	"  - interface: specify the controller id\n"
	"e.g.\n"
	"aicupg\n"
	"  - if no parameter is provided, it will reboot to BROM's upgmode.\n"
	"aicupg usb 0\n"
	"aicupg mmc 1\n"
	"- when devtype is fat: \n"
	"[devtype] [blkdev] [interface]\n"
	"- blkdev: should be udisk,mmc \n"
	"e.g. \n"
	"aicupg fat udisk 0\n"
	"aicupg fat mmc 1\n"
	"aicupg\n"
);
