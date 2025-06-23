/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021-2024 ArtInChip Technology Co., Ltd
 */
#ifndef __AICUPG_H__
#define __AICUPG_H__

#include <linux/compiler.h>
#include <linux/sizes.h>
#include <mmc.h>
#include <fat.h>
#include <fs.h>

#define UPG_PROTO_CMD_GET_HWINFO		0x00
#define UPG_PROTO_CMD_GET_TRACEINFO		0x01
#define UPG_PROTO_CMD_WRITE			0x02
#define UPG_PROTO_CMD_READ			0x03
#define UPG_PROTO_CMD_EXEC			0x04
#define UPG_PROTO_CMD_RUN_SHELL_STR		0x05
#define UPG_PROTO_CMD_GET_MEM_BUF		0x08
#define UPG_PROTO_CMD_FREE_MEM_BUF		0x09
#define UPG_PROTO_CMD_SET_UPG_CFG		0x0A
#define UPG_PROTO_CMD_SET_UPG_END		0x0B
#define UPG_PROTO_CMD_GET_LOG_SIZE		0x0C
#define UPG_PROTO_CMD_GET_LOG_DATA		0x0D
#define UPG_PROTO_CMD_SET_FWC_META		0x10
#define UPG_PROTO_CMD_GET_BLOCK_SIZE		0x11
#define UPG_PROTO_CMD_SEND_FWC_DATA		0x12
#define UPG_PROTO_CMD_GET_FWC_CRC		0x13
#define UPG_PROTO_CMD_GET_FWC_BURN_RESULT	0x14
#define UPG_PROTO_CMD_GET_FWC_RUN_RESULT	0x15
#define UPG_PROTO_CMD_GET_STORAGE_MEDIA		0x16
#define UPG_PROTO_CMD_GET_PARTITION_TABLE	0x17
#define UPG_PROTO_CMD_READ_FWC_DATA		0x18
#define UPG_PROTO_CMD_INVALID			0xFF

#define UPG_PROTO_TYPE                    0x01
#define UPG_PROTO_VERSION                 0x01

/* "UPGC" */
#define UPG_CMD_HEADER_MAGIC              (0x43475055)
/* "UPGR" */
#define UPG_CMD_RESP_MAGIC                (0x52475055)

#define DATA_WRITE_ONCE_SIZE              0x100000

struct cmd_header {
	u32 magic;              /* "UPGC" */
	u8  protocol;
	u8  version;
	u8  command;
	u8  reserved;
	u32 data_length;        /* Command r/w data length */
	u32 checksum;           /* Checksum for this header */
};

struct resp_header {
	u32 magic;              /* "UPGR" */
	u8  protocol;
	u8  version;
	u8  command;            /* Response for specific commmand */
	u8  status;
	u32 data_length;        /* Response data length */
	u32 checksum;           /* Checksum for this header */
};

struct image_header_upgrade {
	char magic[8];
	char platform[64];
	char product[64];
	char version[64];
	char media_type[64];
	uint32_t media_dev_id;
	char media_id[64];
	uint32_t meta_offset;
	uint32_t meta_size;
	uint32_t file_offset;
	uint32_t file_size;
};

struct image_header_pack {
	struct image_header_upgrade hdr;
	char pad[2048 - sizeof(struct image_header_upgrade)];
};

enum upg_mode {
	UPG_MODE_FULL_DISK_UPGRADE = 0x00,
	UPG_MODE_PARTITION_UPGRADE,
	UPG_MODE_BURN_USER_ID,
	UPG_MODE_DUMP_PARTITION,
	UPG_MODE_BURN_IMG_FORCE,
	/*
	 * UPG_MODE_BURN_FROZEN:
	 * Set by Host Tool for case:
	 *   Host Tool already burn image/userid successfuly, and don't want burn
	 *   the same image again before next reboot, Host Tool will set this
	 *   flag to upg_cfg.mode.
	 *
	 *   Host Tool will check this flag from device, if the flag in device is
	 *   UPG_MODE_BURN_FROZEN, it will skip the device.
	 */
	UPG_MODE_BURN_FROZEN,
	UPG_MODE_INVALID,
};

struct upg_cfg {
	u8 mode; /* The value is "enum upg_mode" */
	u8 reserved[31];
};

#define INIT_MODE(mode) (1 << (mode))

struct upg_init {
	/* Store the "enum upg_mode" value by bit index */
	u8 mode_bits;
};

s32 aicupg_initialize(struct upg_init *param);
s32 aicupg_set_upg_cfg(struct upg_cfg *cfg);
s32 aicupg_get_upg_mode(void);
s32 aicupg_data_packet_write(u8 *data, s32 len);
s32 aicupg_data_packet_read(u8 *data, s32 len);
void aicupg_show_upg_cfg_mode(int mode);
void aicupg_show_init_cfg_mode(int mode_bits);

/*SD card upgrade function*/
s32 aicupg_sd_write(struct image_header_upgrade *header, struct mmc *mmc,
		    struct disk_partition part_info);
s32 aicupg_mmc_create_gpt_part(u32 mmc_id, bool is_sdupg);

/*fat upgrade function*/
s32 aicupg_fat_write(char *image_name, char *protection,
				struct image_header_upgrade *header);
typedef void (*progress_cb)(u32 percent);
void aicupg_fat_set_process_cb(progress_cb cb);

#endif /* __AICUPG_H__ */
