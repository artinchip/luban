/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */
#ifndef __AIC_UPG_INTERNAL_H__
#define __AIC_UPG_INTERNAL_H__

#include <linux/kernel.h>
#include <linux/sizes.h>
#include <malloc.h>
#include <artinchip/aicupg.h>
#include <u-boot/crc.h>

#define UPG_RESP_OK                  0
#define UPG_RESP_FAIL                1
#define DEFAULT_BLOCK_ALIGNMENT_SIZE (64 * 1024)

#define cmd_state_init(cmd, init)                                              \
	{                                                                      \
		cmd->state = init;                                             \
	}

#define cmd_state_set_next(cmd, next)                                          \
	{                                                                      \
		cmd->state = next;                                             \
	}

#define FWC_ATTR_REQUIRED    0x00000001
#define FWC_ATTR_OPTIONAL    0x00000002
#define FWC_ATTR_ACTION_RUN  0x00000004
#define FWC_ATTR_ACTION_BURN 0x00000008
#define FWC_ATTR_DEV_BLOCK   0x00000010
#define FWC_ATTR_DEV_MTD     0x00000020
#define FWC_ATTR_DEV_UBI     0x00000040

#define ROUNDUP(x, y)	(((x) + ((y) - 1)) & ~((y) - 1))

enum upg_cmd_state {
	CMD_STATE_IDLE,
	CMD_STATE_START,
	CMD_STATE_ARG,
	CMD_STATE_DATA_IN,
	CMD_STATE_RESP,
	CMD_STATE_DATA_OUT,
	CMD_STATE_RUN,
	CMD_STATE_END,
};

enum upg_dev_type {
	UPG_DEV_TYPE_RAM = 0,
	UPG_DEV_TYPE_MMC,
	UPG_DEV_TYPE_SPI_NAND,
	UPG_DEV_TYPE_SPI_NOR,
	UPG_DEV_TYPE_RAW_NAND,
	UPG_DEV_TYPE_UNKNOWN,
};

struct upg_cmd {
	u32 cmd;
	void (*start)(struct upg_cmd *cmd, s32 data_len);
	s32 (*write_input_data)(struct upg_cmd *cmd, u8 *data, s32 len);
	s32 (*read_output_data)(struct upg_cmd *cmd, u8 *data, s32 len);
	void (*end)(struct upg_cmd *cmd);
	void *priv;
	u32 state;
};

struct upg_internal {
	struct upg_cmd *cur_cmd;
	int dev_type;
	int dev_id;
	struct upg_cfg cfg;
	struct upg_init init;
};

/* FWC meta data will bie aligned to 512 */
#define FWC_META_DATA_LEN 512
#define FWC_STR_MAX_LEN   64
#define PARTITION_TABLE_LEN	128
struct aicupg_partition {
	char name[32];
	u64 start;
	u64 size;
};

struct storage_media {
	char media_type[FWC_STR_MAX_LEN];
	u32 media_dev_id;
};

struct media_partition {
	struct storage_media media;
	struct aicupg_partition part;
	char attr[FWC_STR_MAX_LEN];
};

struct fwc_meta {
	char magic[8]; /* "META" */
	char name[FWC_STR_MAX_LEN]; /* Firmware component name */
	char partition[FWC_STR_MAX_LEN]; /* Partition this FWC belong to */
	u32  offset; /* FWC data offset in Firmware image */
	u32  size;   /* FWC data size */
	u32  crc; /* FWC data CRC32 value */
	u32  ram; /* FWC data download place if it is store in RAM only */
	char attr[FWC_STR_MAX_LEN]; /* Attribute of FWC */
	char padding[296]; /* Aligned to 512 */
};

struct fwc_info {
	struct fwc_meta meta;
	struct media_partition mpart;
	u32 block_size;
	u32 trans_size;
	u32 calc_trans_crc;
	u32 calc_partition_crc;
	s32 burn_result;
	s32 run_result;
	void *priv;
};

void aicupg_gen_resp(struct resp_header *h, u8 cmd, u8 sts, u32 len);
enum upg_dev_type get_current_device_type(void);
void set_current_device_type(enum upg_dev_type type);
const char *get_current_device_name(enum upg_dev_type type);
struct upg_cmd *get_current_command(void);
enum upg_cmd_state get_current_command_state(void);
void set_current_command(struct upg_cmd *cmd);
int get_current_device_id(void);
void set_current_device_id(int id);
int aicupg_get_fwc_attr(struct fwc_info *fwc);

struct upg_cmd *find_basic_command(struct cmd_header *h);
struct upg_cmd *find_fwc_command(struct cmd_header *h);

enum upg_dev_type get_media_type(struct fwc_info *fwc);
bool get_nand_prepare_status(void);
char *mtd_ubi_env_get(void);

void ram_fwc_start(struct fwc_info *fwc);
void mmc_fwc_start(struct fwc_info *fwc);
void nand_fwc_start(struct fwc_info *fwc);
void nor_fwc_start(struct fwc_info *fwc);

s32 ram_fwc_data_write(struct fwc_info *fwc, u8 *buf, s32 len);
s32 mmc_fwc_data_write(struct fwc_info *fwc, u8 *buf, s32 len);
s32 nand_fwc_data_write(struct fwc_info *fwc, u8 *buf, s32 len);
s32 nor_fwc_data_write(struct fwc_info *fwc, u8 *buf, s32 len);

s32 mmc_fwc_data_read(struct fwc_info *fwc, u8 *buf, s32 len);
s32 nand_fwc_data_read(struct fwc_info *fwc, u8 *buf, s32 len);
s32 nor_fwc_data_read(struct fwc_info *fwc, u8 *buf, s32 len);

void ram_fwc_data_end(struct fwc_info *fwc);
void mmc_fwc_data_end(struct fwc_info *fwc);
void nand_fwc_data_end(struct fwc_info *fwc);
void nor_fwc_data_end(struct fwc_info *fwc);

s32 mmc_fwc_prepare(struct fwc_info *fwc, u32 mmc_id);
s32 nand_fwc_prepare(struct fwc_info *fwc, u32 id);
s32 nor_fwc_prepare(struct fwc_info *fwc, u32 id);

s32 mmc_is_exist(struct fwc_info *fwc, u32 mmc_id);
s32 nand_is_exist(void);
s32 nor_is_exist(void);

void fwc_meta_config(struct fwc_info *fwc, struct fwc_meta *pmeta);
s32 media_device_prepare(struct fwc_info *fwc, struct image_header_upgrade *header);
void media_data_write_start(struct fwc_info *fwc);
s32 media_data_write(struct fwc_info *fwc, u8 *buf, u32 len);
void media_data_write_end(struct fwc_info *fwc);

#endif /* __AIC_UPG_INTERNAL_H__ */
