// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd
 */
#include <common.h>
#include <command.h>
#include <artinchip/aicupg.h>
#include "upg_internal.h"

// #undef debug
// #define debug printf

#define BOOT_STAGE_UBOOT 1
/*
 * UPG_PROTO_CMD_GET_HWINFO at BROM stage is use to provide hardware data
 * to Host tool, at U-Boot stage, only used to provide boot_stage information.
 */
struct hwinfo {
	u8 magic[8];
	u8 reserved0[30];
	u8 init_mode;
	u8 curr_mode;
	u8 boot_stage;
	u8 reserved1[67];
};

struct cmd_rw_priv {
	u32 addr;
	u32 len;
	u32 index;
};

/*
 * UPG_PROTO_CMD_GET_HWINFO:
 *   -> [CMD HEADER]
 *   <- [RESP HEADER]
 *   <- [HWINFO DATA]
 */
static void CMD_GET_HWINFO_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
	debug("%s\n", __func__);
	if (cmd->cmd != UPG_PROTO_CMD_GET_HWINFO)
		return;
	cmd_state_init(cmd, CMD_STATE_START);
}

static s32 CMD_GET_HWINFO_write_input_data(struct upg_cmd *cmd, u8 *buf,
					   s32 len)
{
	/* No input data */
	return 0;
}

static s32 CMD_GET_HWINFO_read_output_data(struct upg_cmd *cmd, u8 *buf,
					   s32 len)
{
	struct resp_header resp;
	struct hwinfo hw;
	s32 siz = 0;
	extern struct upg_internal upg_info;

	debug("%s\n", __func__);
	if (cmd->state == CMD_STATE_START)
		cmd_state_set_next(cmd, CMD_STATE_RESP);

	if (cmd->state == CMD_STATE_RESP) {
		/*
		 * Enter read RESP state, to make it simple, HOST should read
		 * RESP in one read operation.
		 */
		siz = sizeof(struct hwinfo); /* Data length */
		aicupg_gen_resp(&resp, cmd->cmd, UPG_RESP_OK, siz);

		siz = sizeof(struct resp_header);
		memcpy(buf, &resp, siz);
		cmd_state_set_next(cmd, CMD_STATE_DATA_OUT);
	}
	if (siz == len)
		return siz;

	if (cmd->state == CMD_STATE_DATA_OUT) {
		/*
		 * Enter read DATA state, to make it simple, HOST should read
		 * HWINFO in one read operation.
		 */
		memset(&hw, 0, sizeof(hw));
		memcpy(hw.magic, "HWINFO", 6);
		hw.boot_stage = BOOT_STAGE_UBOOT;
		hw.init_mode = upg_info.init.mode;
		hw.curr_mode = upg_info.cfg.mode;
		memcpy(buf + siz, &hw, sizeof(hw));
		siz += sizeof(hw);

		cmd_state_set_next(cmd, CMD_STATE_END);
	}

	return siz;
}

static void CMD_GET_HWINFO_end(struct upg_cmd *cmd)
{
	debug("%s\n", __func__);
	cmd_state_set_next(cmd, CMD_STATE_IDLE);
}

static void CMD_UNSUPPORTED_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
}

static s32 CMD_UNSUPPORTED_write_input_data(struct upg_cmd *cmd, u8 *buf,
					    s32 len)
{
	return len;
}

static s32 CMD_UNSUPPORTED_read_output_data(struct upg_cmd *cmd, u8 *buf,
					    s32 len)
{
	struct resp_header resp;
	s32 siz;

	/*
	 * Enter read RESP state, to make it simple, HOST should read
	 * RESP in one read operation.
	 */
	siz = sizeof(struct hwinfo); /* Data length */
	aicupg_gen_resp(&resp, cmd->cmd, UPG_RESP_FAIL, siz);

	siz = sizeof(struct resp_header);
	memcpy(buf, &resp, siz);

	return siz;
}

static void CMD_UNSUPPORTED_end(struct upg_cmd *cmd)
{
}

/*
 * UPG_PROTO_CMD_WRITE:
 *   -> [CMD HEADER]
 *   -> [DATA from host]
 *   <- [RESP HEADER]
 */
static void CMD_WRITE_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
	static struct cmd_rw_priv write_info;

	debug("%s\n", __func__);
	if (cmd->cmd != UPG_PROTO_CMD_WRITE)
		return;
	write_info.addr = 0;
	write_info.len = 0;
	write_info.index = 0;
	cmd->priv = &write_info;
	cmd_state_init(cmd, CMD_STATE_START);
}

static void *hw_friendly_memcpy(void *dest, const void *src, size_t count)
{
	u32 *dl = (u32 *)dest, *sl = (u32 *)src;
	char *d8, *s8;

	if (src == dest)
		return dest;

	/* while all data is aligned (common case), copy a word at a time */
	if ((((unsigned long)dest | (unsigned long)src) & (sizeof(*dl) - 1)) ==
	    0) {
		while (count >= sizeof(*dl)) {
			*dl++ = *sl++;
			count -= sizeof(*dl);
		}
	}
	/* copy the reset one byte at a time */
	d8 = (char *)dl;
	s8 = (char *)sl;
	while (count--)
		*d8++ = *s8++;

	return dest;
}

static s32 CMD_WRITE_write_input_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct cmd_rw_priv *priv;
	u32 val, clen;
	u8 *p;

	clen = 0;
	priv = (struct cmd_rw_priv *)cmd->priv;
	if (priv == NULL)
		return 0;

	if (cmd->state == CMD_STATE_START)
		cmd_state_set_next(cmd, CMD_STATE_ARG);

	if (cmd->state == CMD_STATE_ARG) {
		/*
		 * Enter recv argument state
		 */
		if (len < 8)
			return 0;
		memcpy(&val, buf, 4);
		priv->addr = val; /* Dest address */
		memcpy(&val, buf + 4, 4);
		priv->len = val; /* Data length */
		clen += 8;
		cmd_state_set_next(cmd, CMD_STATE_DATA_IN);
	}

	if (clen == len)
		return clen;
	if (cmd->state == CMD_STATE_DATA_IN) {
		/*
		 * Enter recv data state
		 */
		p = ((u8 *)(unsigned long)priv->addr + priv->index);
		hw_friendly_memcpy(p, buf + clen, (len - clen));
		priv->index += (len - clen);
		clen += (len - clen);

		if (priv->index >= priv->len)
			cmd_state_set_next(cmd, CMD_STATE_RESP);
	}
	return clen;
}

static s32 CMD_WRITE_read_output_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct cmd_rw_priv *priv;
	struct resp_header resp;
	u32 siz = 0;
	u8 sts;

	priv = (struct cmd_rw_priv *)cmd->priv;
	if (priv == NULL)
		return 0;
	if (cmd->state == CMD_STATE_RESP) {
		/*
		 * Enter read RESP state, to make it simple, HOST should read
		 * RESP in one read operation.
		 */
		sts = (priv->index >= priv->len) ? UPG_RESP_OK : UPG_RESP_FAIL;
		aicupg_gen_resp(&resp, cmd->cmd, sts, 0);
		siz = sizeof(struct resp_header);
		memcpy(buf, &resp, siz);
		cmd_state_set_next(cmd, CMD_STATE_END);
	}
	return siz;
}

static void CMD_WRITE_end(struct upg_cmd *cmd)
{
	struct cmd_rw_priv *priv;

	priv = (struct cmd_rw_priv *)cmd->priv;
	if (priv == NULL)
		return;

	if (cmd->state == CMD_STATE_END) {
		priv->addr = 0;
		priv->len = 0;
		priv->index = 0;
		cmd->priv = 0;
		cmd_state_set_next(cmd, CMD_STATE_IDLE);
	}
}

/*
 * UPG_PROTO_CMD_READ:
 *   -> [CMD HEADER]
 *   <- [RESP HEADER]
 *   <- [DATA to host]
 */
static void CMD_READ_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
	static struct cmd_rw_priv read_info;

	debug("%s\n", __func__);
	if (cmd->cmd != UPG_PROTO_CMD_READ)
		return;
	read_info.addr = 0;
	read_info.len = 0;
	read_info.index = 0;
	cmd->priv = &read_info;
	cmd_state_init(cmd, CMD_STATE_START);
}

static s32 CMD_READ_write_input_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct cmd_rw_priv *priv;
	u32 val, clen;

	clen = 0;
	priv = (struct cmd_rw_priv *)cmd->priv;
	if (priv == NULL)
		return 0;

	if (cmd->state == CMD_STATE_START)
		cmd_state_set_next(cmd, CMD_STATE_ARG);

	if (cmd->state == CMD_STATE_ARG) {
		/*
		 * Enter recv argument state
		 */
		if (len < 8)
			return 0;
		memcpy(&val, buf, 4);
		priv->addr = val; /* Read address */
		memcpy(&val, buf + 4, 4);
		priv->len = val; /* Data length */
		clen += 8;
		cmd_state_set_next(cmd, CMD_STATE_RESP);
	}

	return clen;
}

static s32 CMD_READ_read_output_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct cmd_rw_priv *priv;
	struct resp_header resp;
	u32 siz = 0;
	u8 *p;

	priv = (struct cmd_rw_priv *)cmd->priv;
	if (priv == NULL)
		return 0;
	if (cmd->state == CMD_STATE_RESP) {
		/*
		 * Enter read RESP state, to make it simple, HOST should read
		 * RESP in one read operation.
		 */
		siz = priv->len;
		aicupg_gen_resp(&resp, cmd->cmd, 0, siz);
		siz = sizeof(struct resp_header);
		memcpy(buf, &resp, siz);
		cmd_state_set_next(cmd, CMD_STATE_DATA_OUT);
	}
	if (siz == len)
		return siz;
	if (cmd->state == CMD_STATE_DATA_OUT) {
		/* Enter read DATA state */
		p = (u8 *)(unsigned long)priv->addr + priv->index;
		hw_friendly_memcpy(buf + siz, p, (len - siz));
		priv->index += (len - siz);
		siz += (len - siz);
		if (priv->index >= priv->len)
			cmd_state_set_next(cmd, CMD_STATE_END);
	}
	return siz;
}

static void CMD_READ_end(struct upg_cmd *cmd)
{
	struct cmd_rw_priv *priv;

	priv = (struct cmd_rw_priv *)cmd->priv;
	if (priv == NULL)
		return;

	if (cmd->state == CMD_STATE_END) {
		priv->addr = 0;
		priv->len = 0;
		priv->index = 0;
		cmd->priv = 0;
		cmd_state_set_next(cmd, CMD_STATE_IDLE);
	}
}

/*
 * UPG_PROTO_CMD_EXEC:
 *   -> [CMD HEADER]
 *   -> [DATA from host]
 *   <- [RESP HEADER]
 */

typedef int (*exec_func)(void);

static void CMD_EXEC_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
	if (cmd->cmd != UPG_PROTO_CMD_EXEC)
		return;
	cmd->priv = 0;
	cmd_state_init(cmd, CMD_STATE_START);
}

static s32 CMD_EXEC_write_input_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	u32 addr = 0, clen = 0;
	exec_func fn;

	if (cmd->state == CMD_STATE_START)
		cmd_state_set_next(cmd, CMD_STATE_ARG);

	if (cmd->state == CMD_STATE_ARG) {
		/*
		 * Enter recv argument state
		 */
		if (len < 4)
			return 0;
		memcpy(&addr, buf, 4);
		clen = 4;
		cmd_state_set_next(cmd, CMD_STATE_RESP);
	}

	if (addr == 0) {
		cmd->priv = (void *)UPG_RESP_FAIL; /* CMD execute error */
		goto out;
	}

	fn = (exec_func)(unsigned long)addr;
	if (fn())
		cmd->priv = (void *)UPG_RESP_FAIL;
out:
	return clen;
}

static s32 CMD_EXEC_read_output_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct resp_header resp;
	u32 siz = 0;

	if (cmd->state == CMD_STATE_RESP) {
		/*
		 * Enter read RESP state, to make it simple, HOST should read
		 * RESP in one read operation.
		 */
		aicupg_gen_resp(&resp, cmd->cmd, (unsigned long)cmd->priv, 0);
		siz = sizeof(struct resp_header);
		cmd_state_set_next(cmd, CMD_STATE_END);
	}

	return siz;
}

static void CMD_EXEC_end(struct upg_cmd *cmd)
{
	if (cmd->state == CMD_STATE_END) {
		cmd->priv = 0;
		cmd_state_set_next(cmd, CMD_STATE_IDLE);
	}
}

#define MAX_SHELL_CMD_STR_LEN 128
static struct run_shell_info {
	unsigned long cmdlen;
	unsigned long result;
	char shell_str[MAX_SHELL_CMD_STR_LEN];
} shell_info;

static void CMD_RUN_SHELL_STR_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
	if (cmd->cmd != UPG_PROTO_CMD_RUN_SHELL_STR)
		return;
	cmd->priv = &shell_info;
	memset(cmd->priv, 0, sizeof(struct run_shell_info));
	cmd_state_init(cmd, CMD_STATE_START);
}

static s32 CMD_RUN_SHELL_STR_write_input_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct run_shell_info *shinfo;
	u32 clen = 0;

	shinfo = (struct run_shell_info *)cmd->priv;
	if (shinfo == NULL)
		return 0;
	if (cmd->state == CMD_STATE_START)
		cmd_state_set_next(cmd, CMD_STATE_ARG);

	if (cmd->state == CMD_STATE_ARG) {
		/*
		 * Enter recv argument state
		 */
		if (len < 4)
			return 0;
		memcpy(&shinfo->cmdlen, buf, 4);
		clen += 4;
		cmd_state_set_next(cmd, CMD_STATE_DATA_IN);
	}

	if (clen == len)
		return clen;

	if (cmd->state == CMD_STATE_DATA_IN) {
		/*
		 * Enter recv data state, all command string should be sent in
		 * one packet.
		 */

		if (((len - clen) != shinfo->cmdlen) ||
		    (shinfo->cmdlen >= MAX_SHELL_CMD_STR_LEN)) {
			shinfo->result = UPG_RESP_FAIL;
			clen += (len - clen);
			cmd_state_set_next(cmd, CMD_STATE_RESP);
			return clen;
		}
		memcpy(shinfo->shell_str, buf + clen, shinfo->cmdlen);
		clen += shinfo->cmdlen;

		if (run_command(shinfo->shell_str, 0))
			shinfo->result = UPG_RESP_FAIL;

		cmd_state_set_next(cmd, CMD_STATE_RESP);
	}
	return clen;
}

static s32 CMD_RUN_SHELL_STR_read_output_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct run_shell_info *shinfo;
	struct resp_header resp;
	u32 siz = 0;

	shinfo = (struct run_shell_info *)cmd->priv;
	if (shinfo == NULL)
		return 0;

	if (cmd->state == CMD_STATE_RESP) {
		/*
		 * Enter read RESP state, to make it simple, HOST should read
		 * RESP in one read operation.
		 */
		aicupg_gen_resp(&resp, cmd->cmd, shinfo->result, 0);
		siz = sizeof(struct resp_header);
		cmd_state_set_next(cmd, CMD_STATE_END);
	}

	return siz;
}

static void CMD_RUN_SHELL_STR_end(struct upg_cmd *cmd)
{
	if (cmd->state == CMD_STATE_END) {
		cmd->priv = 0;
		cmd_state_set_next(cmd, CMD_STATE_IDLE);
	}
}

static void CMD_GET_MEM_BUF_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
	if (cmd->cmd != UPG_PROTO_CMD_GET_MEM_BUF)
		return;
	cmd->priv = NULL;
	cmd_state_init(cmd, CMD_STATE_START);
}

static s32 CMD_GET_MEM_BUF_write_input_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	u32 memlen, clen = 0;

	if (cmd->state == CMD_STATE_START)
		cmd_state_set_next(cmd, CMD_STATE_ARG);

	if (cmd->state == CMD_STATE_ARG) {
		/*
		 * Enter recv argument state
		 */
		if (len < 4)
			return 0;
		memcpy(&memlen, buf, 4);
		cmd->priv = (void *)(long)memlen;
		clen += 4;
		/* There is no data for this command */
		cmd_state_set_next(cmd, CMD_STATE_RESP);
	}

	return clen;
}

static s32 CMD_GET_MEM_BUF_read_output_data(struct upg_cmd *cmd, u8 *buf,
					    s32 len)
{
	struct resp_header resp;
	u32 siz = 0, memlen;
	u8 *mem = NULL;

	if (!cmd->priv)
		return 0;

	if (cmd->state == CMD_STATE_RESP) {
		memlen = (u32)(long)cmd->priv;
		mem = memalign(ARCH_DMA_MINALIGN, memlen);
		/*
		 * Enter read RESP state, to make it simple, HOST should read
		 * RESP in one read operation.
		 */
		if (mem)
			cmd->priv = mem;
		else
			cmd->priv = NULL;
		aicupg_gen_resp(&resp, cmd->cmd, UPG_RESP_OK, 4);
		siz = sizeof(struct resp_header);
		memcpy(buf, &resp, siz);
		cmd_state_set_next(cmd, CMD_STATE_DATA_OUT);
	}

	if (siz == len)
		return siz;

	if (cmd->state == CMD_STATE_DATA_OUT) {
		/*
		 * Enter read DATA state, to make it simple, HOST should read
		 * data in one read operation.
		 */
		memcpy(buf + siz, &cmd->priv, 4);
		siz += 4;
		cmd_state_set_next(cmd, CMD_STATE_END);
	}
	return siz;
}

static void CMD_GET_MEM_BUF_end(struct upg_cmd *cmd)
{
	if (cmd->state == CMD_STATE_END) {
		cmd->priv = 0;
		cmd_state_set_next(cmd, CMD_STATE_IDLE);
	}
}


static void CMD_FREE_MEM_BUF_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
	if (cmd->cmd != UPG_PROTO_CMD_FREE_MEM_BUF)
		return;
	cmd->priv = NULL;
	cmd_state_init(cmd, CMD_STATE_START);
}

static s32 CMD_FREE_MEM_BUF_write_input_data(struct upg_cmd *cmd, u8 *buf,
					     s32 len)
{
	u32 clen = 0;
	u32 addr = 0;

	if (cmd->state == CMD_STATE_START)
		cmd_state_set_next(cmd, CMD_STATE_ARG);

	if (cmd->state == CMD_STATE_ARG) {
		/*
		 * Enter recv argument state
		 */
		if (len < 4)
			return 0;
		memcpy(&addr, buf, 4);
		clen += 4;

		free((void *)(long)addr);
		cmd_state_set_next(cmd, CMD_STATE_RESP);
	}

	return clen;
}

static s32 CMD_FREE_MEM_BUF_read_output_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct resp_header resp;
	u32 siz = 0;

	if (cmd->state == CMD_STATE_RESP) {
		/*
		 * Enter read RESP state, to make it simple, HOST should read
		 * RESP in one read operation.
		 */
		aicupg_gen_resp(&resp, cmd->cmd, UPG_RESP_OK, 0);
		siz = sizeof(struct resp_header);
		memcpy(buf, &resp, siz);
		cmd_state_set_next(cmd, CMD_STATE_END);
	}

	return siz;
}

static void CMD_FREE_MEM_BUF_end(struct upg_cmd *cmd)
{
	if (cmd->state == CMD_STATE_END) {
		cmd->priv = 0;
		cmd_state_set_next(cmd, CMD_STATE_IDLE);
	}
}

static void CMD_SET_UPG_CFG_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
	if (cmd->cmd != UPG_PROTO_CMD_SET_UPG_CFG)
		return;
	cmd->priv = NULL;
	cmd_state_init(cmd, CMD_STATE_START);
}

static s32 CMD_SET_UPG_CFG_write_input_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	u32 cfglen, clen = 0;
	struct upg_cfg cfg;

	if (cmd->state == CMD_STATE_START)
		cmd_state_set_next(cmd, CMD_STATE_ARG);

	if (cmd->state == CMD_STATE_ARG) {
		/*
		 * Enter recv argument state
		 */
		if (len < 4)
			return 0;
		memcpy(&cfglen, buf, 4);
		clen += 4;
		cmd->priv = (void *)(long)cfglen;
		cmd_state_set_next(cmd, CMD_STATE_DATA_IN);
	}

	if (clen == len)
		return clen;

	if (cmd->state == CMD_STATE_DATA_IN) {
		/*
		 * Enter recv data state, all command string should be sent in
		 * one packet.
		 */

		cfglen = (u32)(long)cmd->priv;
		if (cfglen != sizeof(cfg)) {
			clen += (len - clen);
			cmd_state_set_next(cmd, CMD_STATE_RESP);
			pr_err("Error, Size of upg_cfg is not matched.\n");
			return clen;
		}
		memcpy(&cfg, buf, cfglen);
		clen += cfglen;
		aicupg_set_upg_cfg(&cfg);

		cmd_state_set_next(cmd, CMD_STATE_RESP);
	}
	return clen;
}

static s32 CMD_SET_UPG_CFG_read_output_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct resp_header resp;
	u32 siz = 0;

	if (cmd->state == CMD_STATE_RESP) {
		/*
		 * Enter read RESP state, to make it simple, HOST should read
		 * RESP in one read operation.
		 */
		aicupg_gen_resp(&resp, cmd->cmd, UPG_RESP_OK, 0);
		siz = sizeof(struct resp_header);
		cmd_state_set_next(cmd, CMD_STATE_END);
	}

	return siz;
}

static void CMD_SET_UPG_CFG_end(struct upg_cmd *cmd)
{
	if (cmd->state == CMD_STATE_END) {
		cmd->priv = 0;
		cmd_state_set_next(cmd, CMD_STATE_IDLE);
	}
}


extern void rtc_upg_succ_cnt(void);
static void CMD_SET_UPG_END_start(struct upg_cmd *cmd, s32 cmd_data_len)
{
	if (cmd->cmd != UPG_PROTO_CMD_SET_UPG_END)
		return;
	printf("End of upgrading.\n\n");
	cmd->priv = NULL;
	cmd_state_init(cmd, CMD_STATE_START);
	rtc_upg_succ_cnt();
}

static s32 CMD_SET_UPG_END_write_input_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	u32 param_len, clen = 0;

	if (cmd->state == CMD_STATE_START)
		cmd_state_set_next(cmd, CMD_STATE_ARG);

	if (cmd->state == CMD_STATE_ARG) {
		/*
		 * Enter recv argument state
		 */
		if (len < 4)
			return 0;
		memcpy(&param_len, buf, 4);
		clen += 4;
		cmd->priv = (void *)(long)param_len;
		cmd_state_set_next(cmd, CMD_STATE_DATA_IN);
	}

	if (clen == len)
		return clen;

	if (cmd->state == CMD_STATE_DATA_IN) {
		/*
		 * Enter recv data state, all command string should be sent in
		 * one packet.
		 */

		param_len = (u32)(long)cmd->priv;
		if (param_len != 32) {
			clen += (len - clen);
			cmd_state_set_next(cmd, CMD_STATE_RESP);
			pr_err("Error, Size of upg_end param is not matched.\n");
			return clen;
		}
		/* Current, the param is not used, reserved for future */
		clen += param_len;

		/* Do nothing now, maybe it is useful in future. */

		cmd_state_set_next(cmd, CMD_STATE_RESP);
	}
	return clen;
}

static s32 CMD_SET_UPG_END_read_output_data(struct upg_cmd *cmd, u8 *buf, s32 len)
{
	struct resp_header resp;
	u32 siz = 0;

	if (cmd->state == CMD_STATE_RESP) {
		/*
		 * Enter read RESP state, to make it simple, HOST should read
		 * RESP in one read operation.
		 */
		aicupg_gen_resp(&resp, cmd->cmd, UPG_RESP_OK, 0);
		siz = sizeof(struct resp_header);
		cmd_state_set_next(cmd, CMD_STATE_END);
	}

	return siz;
}

static void CMD_SET_UPG_END_end(struct upg_cmd *cmd)
{
	if (cmd->state == CMD_STATE_END) {
		cmd->priv = 0;
		cmd_state_set_next(cmd, CMD_STATE_IDLE);
	}
}

static struct upg_cmd basic_cmd_list[] = {
	{
		UPG_PROTO_CMD_GET_HWINFO,
		CMD_GET_HWINFO_start,
		CMD_GET_HWINFO_write_input_data,
		CMD_GET_HWINFO_read_output_data,
		CMD_GET_HWINFO_end,
	},
	{
		UPG_PROTO_CMD_GET_TRACEINFO,
		CMD_UNSUPPORTED_start,
		CMD_UNSUPPORTED_write_input_data,
		CMD_UNSUPPORTED_read_output_data,
		CMD_UNSUPPORTED_end,
	},
	{
		UPG_PROTO_CMD_WRITE,
		CMD_WRITE_start,
		CMD_WRITE_write_input_data,
		CMD_WRITE_read_output_data,
		CMD_WRITE_end,
	},
	{
		UPG_PROTO_CMD_READ,
		CMD_READ_start,
		CMD_READ_write_input_data,
		CMD_READ_read_output_data,
		CMD_READ_end,
	},
	{
		UPG_PROTO_CMD_EXEC,
		CMD_EXEC_start,
		CMD_EXEC_write_input_data,
		CMD_EXEC_read_output_data,
		CMD_EXEC_end,
	},
	{
		UPG_PROTO_CMD_RUN_SHELL_STR,
		CMD_RUN_SHELL_STR_start,
		CMD_RUN_SHELL_STR_write_input_data,
		CMD_RUN_SHELL_STR_read_output_data,
		CMD_RUN_SHELL_STR_end,
	},
	{
		UPG_PROTO_CMD_SET_UPG_CFG,
		CMD_SET_UPG_CFG_start,
		CMD_SET_UPG_CFG_write_input_data,
		CMD_SET_UPG_CFG_read_output_data,
		CMD_SET_UPG_CFG_end,
	},
	{
		UPG_PROTO_CMD_SET_UPG_END,
		CMD_SET_UPG_END_start,
		CMD_SET_UPG_END_write_input_data,
		CMD_SET_UPG_END_read_output_data,
		CMD_SET_UPG_END_end,
	},
	{
		UPG_PROTO_CMD_GET_MEM_BUF,
		CMD_GET_MEM_BUF_start,
		CMD_GET_MEM_BUF_write_input_data,
		CMD_GET_MEM_BUF_read_output_data,
		CMD_GET_MEM_BUF_end,
	},
	{
		UPG_PROTO_CMD_FREE_MEM_BUF,
		CMD_FREE_MEM_BUF_start,
		CMD_FREE_MEM_BUF_write_input_data,
		CMD_FREE_MEM_BUF_read_output_data,
		CMD_FREE_MEM_BUF_end,
	},
};

struct upg_cmd *find_basic_command(struct cmd_header *h)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(basic_cmd_list); i++) {
		if (basic_cmd_list[i].cmd == (u32)h->command)
			return &basic_cmd_list[i];
	}

	return NULL;
}
