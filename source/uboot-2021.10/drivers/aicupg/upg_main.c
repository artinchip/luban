// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */

#include <common.h>
#include <command.h>
#include <env.h>
#include <artinchip/aicupg.h>
#include "upg_internal.h"

#define MAX_PART_NAME    32
#define GPT_HEADER_SIZE  (34 * 512)
#define GPT_CMD_BUF_SIZE 2048

struct upg_internal upg_info = {
	.cur_cmd = NULL,
	.dev_type = UPG_DEV_TYPE_RAM,
	.dev_id = 0,
	.cfg = {
		.mode = 0,
	}
};

struct aicupg_gpt_partition {
	char name[32];
	u64 start;
	u64 size;
	struct aicupg_gpt_partition *next;
};

static bool __check_cmd_header(struct cmd_header *h)
{
	u32 sum;

	if (h->magic != UPG_CMD_HEADER_MAGIC)
		return false;
	if (h->protocol != UPG_PROTO_TYPE)
		return false;
	if (h->version != UPG_PROTO_VERSION)
		return false;

	sum = 0;
	sum += h->magic;
	sum += ((h->reserved << 24) | (h->command << 16) | (h->version << 8) |
		h->protocol);
	sum += h->data_length;
	if (sum != h->checksum)
		return false;

	return true;
}

void aicupg_gen_resp(struct resp_header *h, u8 cmd, u8 sts, u32 len)
{
	u32 sum;

	h->magic = UPG_CMD_RESP_MAGIC;
	h->protocol = UPG_PROTO_TYPE;
	h->version = UPG_PROTO_VERSION;
	h->command = cmd;
	h->status = sts;
	h->data_length = len;

	sum = 0;
	sum += h->magic;
	sum += ((h->status << 24) | (h->command << 16) | (h->version << 8) |
		h->protocol);
	sum += h->data_length;
	h->checksum = sum;
}

static char *get_upg_mode_name(int mode)
{
	char *modes[] = {
		"Full disk upgrade",
		"Partition upgrade",
		"Burn UserID",
		"Dump partition",
		"Force upgrade",
	};
	char *invalid = "Invalid mode";

	if (mode < 0 || mode >= UPG_MODE_INVALID)
		return invalid;
	return modes[mode];
}

s32 aicupg_set_upg_cfg(struct upg_cfg *cfg)
{
	if (!cfg) {
		pr_info("Invalide parameter.\n");
		return -1;
	}

	memcpy(&upg_info.cfg, cfg, sizeof(*cfg));
	printf("%s, mode = %s\n", __func__,
	       get_upg_mode_name(upg_info.cfg.mode));

	return 0;
}

s32 aicupg_initialize(struct upg_init *param)
{
	upg_info.init.mode = param->mode;
	return 0;
}

s32 aicupg_get_upg_mode(void)
{
	return (s32)upg_info.cfg.mode;
}

void set_current_command(struct upg_cmd *cmd)
{
	upg_info.cur_cmd = cmd;
}

struct upg_cmd *get_current_command(void)
{
	return upg_info.cur_cmd;
}

enum upg_cmd_state get_current_command_state(void)
{
	if (upg_info.cur_cmd)
		return upg_info.cur_cmd->state;
	return CMD_STATE_IDLE;
}

void set_current_device_type(enum upg_dev_type type)
{
	upg_info.dev_type = type;
}

enum upg_dev_type get_current_device_type(void)
{
	return upg_info.dev_type;
}

const char *get_current_device_name(enum upg_dev_type type)
{
	char *dev_list[] = {
	"RAM",
	"MMC",
	"SPI_NAND",
	"SPI_NOR",
	"RAW_NAND",
	"UNKNOWN",
	};

	return dev_list[type];
}

void set_current_device_id(int id)
{
	upg_info.dev_id = id;
}

int get_current_device_id(void)
{
	return upg_info.dev_id;
}

static struct upg_cmd *find_command(struct cmd_header *h)
{
	struct upg_cmd *cmd = NULL;

	cmd = find_basic_command(h);
	if (cmd)
		return cmd;

	/* Not basic command, maybe it is FWC relative command. */
	cmd = find_fwc_command(h);
	return cmd;
}

s32 aicupg_data_packet_write(u8 *data, s32 len)
{
	struct cmd_header h;
	struct upg_cmd *cmd;
	u32 clen;

	clen = 0;
	if (len >= sizeof(struct cmd_header))
		memcpy(&h, data, sizeof(struct cmd_header));

	if ((len >= sizeof(struct cmd_header)) &&
	    (__check_cmd_header(&h) == true)) {
		/*
		 * Command start packet, find the command handler
		 */
		cmd = find_command(&h);
		set_current_command(cmd);
		if (cmd)
			cmd->start(cmd, h.data_length);
		clen = sizeof(struct cmd_header);
	}

	/* Maybe this packet is cmd_header only */
	if (clen == len)
		return clen;
	/*
	 * There is command data
	 */
	cmd = get_current_command();
	if (cmd && cmd->write_input_data)
		clen += cmd->write_input_data(cmd, data, len - clen);

	/* End CMD after CSW is sent */
	if (get_current_command_state() == CMD_STATE_END)
		cmd->end(cmd);

	pr_debug("%s, l: %d\n", __func__, __LINE__);
	return clen;
}

s32 aicupg_data_packet_read(u8 *data, s32 len)
{
	struct upg_cmd *cmd;
	s32 rlen = 0;

	/*
	 * Host read data from device
	 */
	cmd = get_current_command();
	if (cmd && cmd->read_output_data)
		rlen = cmd->read_output_data(cmd, data, len);

	/* End CMD before CSW is sent */
	if (get_current_command_state() == CMD_STATE_END)
		cmd->end(cmd);

	return rlen;
}

int aicupg_get_fwc_attr(struct fwc_info *fwc)
{
	int attr = 0;

	if (!fwc)
		return 0;

	if (strstr(fwc->meta.attr, "required"))
		attr |= FWC_ATTR_REQUIRED;
	else if (strstr(fwc->meta.attr, "optional"))
		attr |= FWC_ATTR_OPTIONAL;

	if (strstr(fwc->meta.attr, "run"))
		attr |= FWC_ATTR_ACTION_RUN;
	else if (strstr(fwc->meta.attr, "burn"))
		attr |= FWC_ATTR_ACTION_BURN;

	if (strstr(fwc->meta.attr, "block"))
		attr |= FWC_ATTR_DEV_BLOCK;
	else if (strstr(fwc->meta.attr, "mtd"))
		attr |= FWC_ATTR_DEV_MTD;
	else if (strstr(fwc->meta.attr, "ubi"))
		attr |= FWC_ATTR_DEV_UBI;

	return attr;
}

/*
 *Init fwc and config fwc->meta
*/
void fwc_meta_config(struct fwc_info *fwc, struct fwc_meta *pmeta)
{
	memset((void *)fwc, 0, sizeof(struct fwc_info));
	memcpy(&fwc->meta, pmeta, sizeof(struct fwc_meta));
}

/*
 *Get memory type by header
 *- Determine the memory type of the current image
*/
static enum upg_dev_type media_type_get(struct image_header_upgrade *header)
{
	static enum upg_dev_type type;
	pr_debug("%s, %s\n", __func__, header->media_type);

	if (strcmp(header->media_type, "mmc") == 0)
		type = UPG_DEV_TYPE_MMC;
	else if (strcmp(header->media_type, "raw-nand") == 0)
		type = UPG_DEV_TYPE_RAW_NAND;
	else if (strcmp(header->media_type, "spi-nand") == 0)
		type = UPG_DEV_TYPE_SPI_NAND;
	else if (strcmp(header->media_type, "spi-nor") == 0)
		type = UPG_DEV_TYPE_SPI_NOR;
	else
		type = UPG_DEV_TYPE_UNKNOWN;

	return type;
}

/*
 *Prepare write data
 *- Select function based on type
*/
s32 media_device_prepare(struct fwc_info *fwc, struct image_header_upgrade *header)
{
	s32 ret = 0;
	enum upg_dev_type type;

	/*get device type*/
	type = media_type_get(header);
	/*config upg_info*/
	set_current_device_type(type);
	set_current_device_id(header->media_dev_id);
	switch (type) {
#ifdef CONFIG_AICUPG_NOR_ARTINCHIP
	case UPG_DEV_TYPE_SPI_NOR:
		ret = nor_fwc_prepare(fwc, header->media_dev_id);
		break;
#endif
#ifdef CONFIG_AICUPG_NAND_ARTINCHIP
	case UPG_DEV_TYPE_SPI_NAND:
		if (get_nand_prepare_status() != true) {
			ret = nand_fwc_prepare(fwc, header->media_dev_id);
		}
		break;
#endif
#ifdef CONFIG_AICUPG_MMC_ARTINCHIP
	case UPG_DEV_TYPE_MMC:
		ret = mmc_fwc_prepare(fwc, header->media_dev_id);
		break;
#endif
	default:
		pr_err("device type is not support!...\n");
		ret = -1;
		break;
	}
	return ret;
}

/*
 *Start write data
 *- Select function based on type
*/
void media_data_write_start(struct fwc_info *fwc)
{
	enum upg_dev_type type;
	type = get_current_device_type();

	switch (type) {
#ifdef CONFIG_AICUPG_NOR_ARTINCHIP
	case UPG_DEV_TYPE_SPI_NOR:
		nor_fwc_start(fwc);
		break;
#endif
#ifdef CONFIG_AICUPG_NAND_ARTINCHIP
	case UPG_DEV_TYPE_SPI_NAND:
		nand_fwc_start(fwc);
		break;
#endif
#ifdef CONFIG_AICUPG_MMC_ARTINCHIP
	case UPG_DEV_TYPE_MMC:
		mmc_fwc_start(fwc);
		break;
#endif
	default:
		pr_err("device type is not support!...\n");
		break;
	}
}

/*
 *Write data to memory device
 *- Make the data size into whole block
 *- Select function based on type
*/
s32 media_data_write(struct fwc_info *fwc, u8 *buf, u32 len)
{
	s32 ret;
	s32 len_to_write;
	enum upg_dev_type type;

	type = get_current_device_type();
	if (len % fwc->block_size)
		len_to_write = len + fwc->block_size - (len % fwc->block_size);
	else
		len_to_write = len;

	switch (type) {
#ifdef CONFIG_AICUPG_NOR_ARTINCHIP
	case UPG_DEV_TYPE_SPI_NOR:
		ret = nor_fwc_data_write(fwc, buf, len_to_write);
		break;
#endif
#ifdef CONFIG_AICUPG_NAND_ARTINCHIP
	case UPG_DEV_TYPE_SPI_NAND:
		ret = nand_fwc_data_write(fwc, buf, len_to_write);
		break;
#endif
#ifdef CONFIG_AICUPG_MMC_ARTINCHIP
	case UPG_DEV_TYPE_MMC:
		ret = mmc_fwc_data_write(fwc, buf, len_to_write);
		break;
#endif
	default:
		ret = 0;
		pr_err("device type is not support!...\n");
		break;
	}
	/*The size of the data we actually write is len*/
	if (ret != len_to_write)
		ret = 0;
	else
		ret = len;
	return ret;
}

/*
 *End write data
 *- Select function based on type
*/
void media_data_write_end(struct fwc_info *fwc)
{
	enum upg_dev_type type;

	type = get_current_device_type();

	switch (type) {
#ifdef CONFIG_AICUPG_NOR_ARTINCHIP
	case UPG_DEV_TYPE_SPI_NOR:
		nor_fwc_data_end(fwc);
		break;
#endif
#ifdef CONFIG_AICUPG_NAND_ARTINCHIP
	case UPG_DEV_TYPE_SPI_NAND:
		nand_fwc_data_end(fwc);
		break;
#endif
#ifdef CONFIG_AICUPG_MMC_ARTINCHIP
	case UPG_DEV_TYPE_MMC:
		mmc_fwc_data_end(fwc);
		break;
#endif
	default:
		pr_err("device type is not support!...\n");
		break;
	}
}

static struct aicupg_gpt_partition *new_partition(char *s, u64 start)
{
	struct aicupg_gpt_partition *part = NULL;
	int cnt = 0;
	char *p;

	part = (struct aicupg_gpt_partition *)malloc(sizeof(struct aicupg_gpt_partition));
	if (!part)
		return NULL;
	memset(part, 0, sizeof(struct aicupg_gpt_partition));

	p = s;
	part->start = start;
	if (*p == '-') {
		/* All remain space */
		part->size = 0;
		p++;
	} else {
		part->size = ustrtoull(p, &p, 0);
	}
	if (*p == '@') {
		p++;
		part->start = ustrtoull(p, &p, 0);
	}
	if (*p != '(') {
		pr_err("Partition name should be next of size.\n");
		goto err;
	}
	p++;

	while (*p != ')') {
		if (cnt >= MAX_PART_NAME)
			break;
		part->name[cnt++] = *p++;
	}
	p++;
	if (*p == ',') {
		p++;
		part->next = new_partition(p, part->start + part->size);
	}
	pr_info("part: %s, start %lld, size %lld\n", part->name, part->start,
		part->size);
	return part;
err:
	if (part)
		free(part);
	return NULL;
}

static void free_gpt_partition(struct aicupg_gpt_partition *part)
{
	struct aicupg_gpt_partition *next;

	if (!part)
		return;

	next = part->next;
	free(part);
	free_gpt_partition(next);
}

s32 aicupg_mmc_create_gpt_part(u32 mmc_id, bool is_sdupg)
{
	struct aicupg_gpt_partition *parts = NULL, *item;
	char *parts_mmc, *cmdbuf, *p;
	int ret = 0, limit;

	/*
	 * Step1: Ensure MMC device is exist
	 */
	if (mmc_id >= get_mmc_num()) {
		pr_err("Invalid mmc dev %d\n", mmc_id);
		return -ENODEV;
	}

	/*
	 * Step2: Get partitions table from ENV
	 * if upgrade         - parts_mmc
	 * if SD card upgrade -burn_mmc
	 * If it is USB upgrade, Host tool already transferred env.bin to DRAM
	 * during BROM stage.
	 */
	if (is_sdupg)
		parts_mmc = env_get("burn_mmc");
	else
		parts_mmc = env_get("GPT");
	if (!parts_mmc) {
		pr_err("Get gpt partition table from ENV failed.\n");
		return -ENODEV;
	}
	parts = new_partition(parts_mmc, (GPT_HEADER_SIZE));
	if (!parts)
		return -1;
	if (parts->start != GPT_HEADER_SIZE) {
		pr_err("First partition start offset is not correct\n");
		return -1;
	}

	cmdbuf = (char *)malloc(GPT_CMD_BUF_SIZE);
	if (!cmdbuf) {
		ret = -1;
		goto out;
	}
	limit = 2048;
	p = cmdbuf;
	snprintf(p, limit, "gpt write mmc %d \"", mmc_id);
	p = cmdbuf + strlen(cmdbuf);
	limit = 2048 - strlen(cmdbuf);
	item = parts;
	while (item) {
		if (item->size > 0)
			snprintf(p, limit, "name=%s,start=%lld,size=%lld;",
				 item->name, item->start, item->size);
		else
			snprintf(p, limit, "name=%s,start=%lld,size=-;\"",
				 item->name, item->start);
		p = cmdbuf + strlen(cmdbuf);
		limit = GPT_CMD_BUF_SIZE - strlen(cmdbuf);
		item = item->next;
	}

	/*
	 * Step3: Create GPT partitions
	 */
	ret = run_command(cmdbuf, 0);
out:
	if (parts)
		free_gpt_partition(parts);
	if (cmdbuf)
		free(cmdbuf);
	return ret;
}

