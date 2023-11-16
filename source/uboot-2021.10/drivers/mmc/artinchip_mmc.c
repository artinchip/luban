// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors: matteo <duanmt@artinchip.com>
 */

#include <bouncebuf.h>
#include <common.h>
#include <cpu_func.h>
#include <errno.h>
#include <malloc.h>
#include <memalign.h>
#include <mmc.h>
#include <wait_bit.h>
#include <power/regulator.h>
#include <clk.h>
#include <dm.h>
#include <dt-structs.h>
#include <reset.h>
#include <pwrseq.h>
#include <syscon.h>
#include <linux/err.h>
#include <dm/device_compat.h>

#include "artinchip_mmc.h"

#define PAGE_SIZE		4096
#define SDMC_TIMEOUT		10000

#define MSEC_PER_SEC		1000

#define CLOCK_MIN		400000	/*  400 kHz */
#define FIFO_MIN		8
#define FIFO_MAX		4096
#define SDMC_THRCTL		0x100
#define SDMC_CARDRDTHR_EN	BIT(0)
#define SDMC_CARDTHR_SHIFT	(16)
#define SDMC_CARDTHR_MASK	(0xfff << SDMC_CARDTHR_SHIFT)

#define DAT0_STATUS_OFS (9)
#define DAT0_STATUS_MSK (0x1 << DAT0_STATUS_OFS)

struct aic_sdmc_plat {
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct dtd_artinchip_aic_sdmc dtplat;
#endif
	struct mmc_config cfg;
	struct mmc mmc;
};

struct aic_sdmc_priv {
	struct aic_sdmc host;
	struct clk clk;
	struct reset_ctl reset;
	int fifo_depth;
	bool fifo_mode;
	u32 f_max;
	u32 f_min;
};

static inline void sdmc_writel(struct aic_sdmc *host, int reg, u32 val)
{
	writel(val, host->ioaddr + reg);
}

static inline u32 sdmc_readl(struct aic_sdmc *host, int reg)
{
	return readl(host->ioaddr + reg);
}

static int aic_sdmc_idma_start(struct aic_sdmc *host, struct mmc_data *data,
			       struct bounce_buffer *bbstate)
{
	u32 size = data->blocksize * data->blocks;

	sdmc_writel(host, SDMC_IDMAINTEN, SDMC_IDMAC_INT_MASK);

	if (data->flags == MMC_DATA_READ)
		return bounce_buffer_start(bbstate, (void *)data->dest,
					   size, GEN_BB_WRITE);
	else
		return bounce_buffer_start(bbstate, (void *)data->src,
					   size, GEN_BB_READ);
}

static int aic_sdmc_idma_stop(struct aic_sdmc *host,
			      struct bounce_buffer *bbstate, u32 flag)
{
	u32 i, mask, ctrl, retry = SDMC_TIMEOUT;

	if (flag == MMC_DATA_READ)
		mask = SDMC_IDMAC_INT_RI;
	else
		mask = SDMC_IDMAC_INT_TI;

	for (i = 0; i < retry; i++) {
		ctrl = sdmc_readl(host, SDMC_IDMAST);
		if (mask & ctrl)
			break;
	}

	ctrl = sdmc_readl(host, SDMC_IDMAST);
	sdmc_writel(host, SDMC_IDMAST, ctrl);

	ctrl = sdmc_readl(host, SDMC_HCTRL1);
	ctrl &= ~(SDMC_HCTRL1_DMA_EN);
	sdmc_writel(host, SDMC_HCTRL1, ctrl);

	ctrl = sdmc_readl(host, SDMC_IDMAINTEN);
	ctrl &= ~(SDMC_INT_ALL);
	sdmc_writel(host, SDMC_IDMAINTEN, ctrl);
	bounce_buffer_stop(bbstate);

	if (i == retry) {
		pr_warn("%s() - %s interrupt timeout.\n", __func__,
			mask == SDMC_IDMAC_INT_RI ? "Rx" : "Tx");
		return -ETIMEDOUT;
	}
	return 0;
}

static void aic_sdmc_idma_prepare_desc(struct aic_sdmc_idma_desc *idmac,
				       u32 flags, u32 cnt, u32 addr)
{
	struct aic_sdmc_idma_desc *desc = idmac;

	desc->flags = flags;
	desc->cnt = cnt;
	desc->addr = addr;
	desc->next_addr = (ulong)desc + sizeof(struct aic_sdmc_idma_desc);
}

static void aic_sdmc_idma_prepare(struct aic_sdmc *host,
				  struct mmc_data *data,
				  struct aic_sdmc_idma_desc *cur_idma,
				  void *bounce_buffer)
{
	unsigned long ctrl;
	unsigned int i = 0, flags, cnt, blk_cnt;
	ulong data_start, data_end;

	blk_cnt = data->blocks;
	sdmc_writel(host, SDMC_IDMAST, 0xFFFFFFFF);

	data_start = (ulong)cur_idma;
	sdmc_writel(host, SDMC_IDMASADDR, (ulong)cur_idma);

	do {
		flags = SDMC_IDMAC_OWN | SDMC_IDMAC_CH;
		flags |= (i == 0) ? SDMC_IDMAC_FS : 0;
		if (blk_cnt <= 8) {
			flags |= SDMC_IDMAC_LD;
			cnt = data->blocksize * blk_cnt;
		} else {
			cnt = data->blocksize * 8;
		}

		aic_sdmc_idma_prepare_desc(cur_idma, flags, cnt,
					   (ulong)bounce_buffer +
					   (i * PAGE_SIZE));

		cur_idma++;
		if (blk_cnt <= 8)
			break;
		blk_cnt -= 8;
		i++;
	} while (1);

	data_end = (ulong)cur_idma;
	flush_dcache_range(data_start, roundup(data_end, ARCH_DMA_MINALIGN));

	ctrl = sdmc_readl(host, SDMC_HCTRL1);
	ctrl |= SDMC_HCTRL1_USE_IDMAC | SDMC_HCTRL1_DMA_EN;
	sdmc_writel(host, SDMC_HCTRL1, ctrl);

	ctrl = sdmc_readl(host, SDMC_PBUSCFG);
	ctrl |= SDMC_PBUSCFG_IDMAC_FB | SDMC_PBUSCFG_IDMAC_EN;
	sdmc_writel(host, SDMC_PBUSCFG, ctrl);
}

static unsigned int aic_sdmc_get_timeout(struct mmc *mmc,
					 const unsigned int size)
{
	unsigned int timeout;

	timeout = size * 8;
	timeout /= mmc->bus_width;
	if (mmc->capacity < 1000000000)
		timeout *= 50;		/* < 1GB, wait 50 times as long */
	else
		timeout *= 10;		/* wait 10 times as long */

	timeout /= (mmc->clock / MSEC_PER_SEC);
	timeout /= mmc->ddr_mode ? 2 : 1;
	timeout = (timeout < MSEC_PER_SEC) ? MSEC_PER_SEC : timeout;
	return timeout;
}

static int aic_sdmc_fifo_rdy(struct aic_sdmc *host, u32 bit, u32 *len)
{
	u32 timeout = SDMC_TIMEOUT;

	*len = sdmc_readl(host, SDMC_CTRST);
	while (--timeout && (*len & bit)) {
		udelay(200);
		*len = sdmc_readl(host, SDMC_CTRST);
	}

	if (!timeout) {
		pr_warn("%s() - FIFO underflow timeout\n", __func__);
		return -ETIMEDOUT;
	}

	return 0;
}

static int aic_sdmc_data_rx(struct aic_sdmc *host, u32 *buf, u32 size)
{
	u32 i, len = 0;

	sdmc_writel(host, SDMC_OINTST, SDMC_INT_RXDR | SDMC_INT_DAT_DONE);
	while (size) {
		if (aic_sdmc_fifo_rdy(host, SDMC_CTRST_FIFO_EMPTY, &len) < 0)
			return size;

		len = SDMC_CTRST_FCNT(len);
		len = min(size, len);
		for (i = 0; i < len; i++)
			*buf++ = sdmc_readl(host, SDMC_FIFO_DATA);
		size = size > len ? (size - len) : 0;
	}

	return size;
}

static int aic_sdmc_data_tx(struct aic_sdmc *host, u32 *buf, u32 size)
{
	u32 i, len = 0;
	u32 fifo_depth = (((host->fifoth_val & RX_WMARK_MASK) >>
			    RX_WMARK_SHIFT) + 1) * 2;

	while (size) {
		if (aic_sdmc_fifo_rdy(host, SDMC_CTRST_FIFO_FULL, &len) < 0)
			return size;

		len = fifo_depth - SDMC_CTRST_FCNT(len);
		len = min(size, len);
		for (i = 0; i < len; i++)
			sdmc_writel(host, SDMC_FIFO_DATA, *buf++);
		size = size > len ? (size - len) : 0;
	}

	sdmc_writel(host, SDMC_OINTST, SDMC_INT_TXDR);
	return size;
}

static int aic_sdmc_data_transfer(struct aic_sdmc *host, struct mmc_data *data)
{
	struct mmc *mmc = host->mmc;
	int ret = 0;
	u32 timeout, mask, size, total;
	ulong start = get_timer(0);

	total = data->blocksize * data->blocks;
	timeout = aic_sdmc_get_timeout(mmc, total);

	size = total / 4;
	for (;;) {
		mask = sdmc_readl(host, SDMC_OINTST);
		if (mask & (SDMC_DATA_ERR | SDMC_DATA_TOUT)) {
			pr_err("%s() Data error! size %d/%d, mode %s\n",
			       __func__, size * 4, total,
				host->fifo_mode ? "FIFO" : "IDMA");
			ret = -EINVAL;
			break;
		}

		if (host->fifo_mode && size) {
			if (data->flags == MMC_DATA_READ &&
			    (mask & (SDMC_INT_RXDR | SDMC_INT_DAT_DONE)))
				size = aic_sdmc_data_rx(host, (u32 *)data->dest, size);
			else if (data->flags == MMC_DATA_WRITE &&
				 (mask & SDMC_INT_TXDR))
				size = aic_sdmc_data_tx(host, (u32 *)data->src, size);
		}

		if (mask & SDMC_INT_DAT_DONE) {
			ret = 0;
			break;
		}

		if (get_timer(start) > timeout) {
			pr_warn("%s() Data timeout %d! size %d/%d, mode %s\n",
				__func__, timeout, size * 4, total,
				host->fifo_mode ? "FIFO" : "IDMA");
			ret = -ETIMEDOUT;
			break;
		}
	}

	sdmc_writel(host, SDMC_OINTST, mask);

	return ret;
}

static int aic_sdmc_set_transfer_mode(struct aic_sdmc *host,
				      struct mmc_data *data)
{
	unsigned long mode = SDMC_CMD_DAT_EXP;

	if (data->flags & MMC_DATA_WRITE)
		mode |= SDMC_CMD_DAT_WR;

	return mode;
}

static int aic_sdmc_reset(struct aic_sdmc *host, u32 value)
{
	unsigned long timeout = SDMC_TIMEOUT;
	u32 ctrl;

	ctrl = sdmc_readl(host, SDMC_HCTRL1);
	ctrl |= value;
	sdmc_writel(host, SDMC_HCTRL1, ctrl);

	while (timeout--) {
		ctrl = sdmc_readl(host, SDMC_HCTRL1);
		if (!(ctrl & value))
			return 0;
		udelay(100);
	}
	return -ETIME;
}

#ifdef CONFIG_DM_MMC
static int aic_sdmc_send_cmd(struct udevice *dev, struct mmc_cmd *cmd,
			     struct mmc_data *data)
{
	struct mmc *mmc = mmc_get_mmc_dev(dev);
#else
static int aic_sdmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd,
			     struct mmc_data *data)
{
#endif
	struct aic_sdmc *host = mmc->priv;

	ALLOC_CACHE_ALIGN_BUFFER(struct aic_sdmc_idma_desc, cur_idma,
				 data ? DIV_ROUND_UP(data->blocks, 8) : 0);
	int ret = 0, flags = 0;
	unsigned int timeout = 500;
	u32 mask;
	ulong start = get_timer(0);
	struct bounce_buffer bbstate;

	if (cmd->cmdidx != MMC_CMD_STOP_TRANSMISSION) {
		while (sdmc_readl(host, SDMC_CTRST) & SDMC_CTRST_BUSY) {
			if (get_timer(start) > timeout) {
				pr_warn("%s() - Data transfer is busy\n",
					__func__);
				return -ETIMEDOUT;
			}
		}
	}

	sdmc_writel(host, SDMC_OINTST, SDMC_INT_ALL);
	if (data) {
		if ((data->blocksize * data->blocks) < 512)
			host->fifo_mode = true;
		else
			host->fifo_mode = false;

		sdmc_writel(host, SDMC_BLKSIZ, data->blocksize);
		sdmc_writel(host, SDMC_BLKCNT, data->blocks);
		aic_sdmc_reset(host, SDMC_HCTRL1_FIFO_RESET);

		if (!host->fifo_mode) {
			ret = aic_sdmc_idma_start(host, data, &bbstate);
			if (ret)
				return ret;

			aic_sdmc_idma_prepare(host, data, cur_idma,
					      bbstate.bounce_buffer);
		}
	}

	sdmc_writel(host, SDMC_CMDARG, cmd->cmdarg);

	if (data)
		flags = aic_sdmc_set_transfer_mode(host, data);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY))
		return -1;

	if (cmd->cmdidx == MMC_CMD_GO_IDLE_STATE)
		flags |= SDMC_CMD_INIT;

	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
		flags |= SDMC_CMD_STOP;
	else
		flags |= SDMC_CMD_PRV_DAT_WAIT;

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		flags |= SDMC_CMD_RESP_EXP;
		if (cmd->resp_type & MMC_RSP_136)
			flags |= SDMC_CMD_RESP_LEN;
	}

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= SDMC_CMD_RESP_CRC;

	flags |= (cmd->cmdidx | SDMC_CMD_START | SDMC_CMD_USE_HOLD_REG);

	sdmc_writel(host, SDMC_CMD, flags);

	start = get_timer(0);
	timeout = 300;
	for (;;) {
		mask = sdmc_readl(host, SDMC_OINTST);
		if (mask & SDMC_INT_CMD_DONE) {
			if (!data)
				sdmc_writel(host, SDMC_OINTST, mask);
			break;
		}
		if (get_timer(start) > timeout) {
		pr_warn("%s()%d - Wait CMD%d done timeout.\n", __func__,
			__LINE__, cmd->cmdidx);
		return -ETIMEDOUT;
		}
	}
	if (mask & SDMC_INT_RTO) {
		pr_debug("%s()%d - CMD%d Response Timeout.\n",
			 __func__, __LINE__, cmd->cmdidx);
		return -ETIMEDOUT;
	} else if (mask & SDMC_INT_RESP_ERR) {
		pr_err("%s()%d - CMD%d Response Error.\n",
		       __func__, __LINE__, cmd->cmdidx);
		return -EIO;
	} else if ((cmd->resp_type & MMC_RSP_CRC) &&
		   (mask & SDMC_INT_RCRC)) {
		pr_err("%s()%d - CMD%d Response CRC Error.\n",
		       __func__, __LINE__, cmd->cmdidx);
		return -EIO;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			cmd->response[0] = sdmc_readl(host, SDMC_RESP3);
			cmd->response[1] = sdmc_readl(host, SDMC_RESP2);
			cmd->response[2] = sdmc_readl(host, SDMC_RESP1);
			cmd->response[3] = sdmc_readl(host, SDMC_RESP0);
		} else {
			cmd->response[0] = sdmc_readl(host, SDMC_RESP0);
		}
	}


	if (data) {
		ret = aic_sdmc_data_transfer(host, data);

		if (!host->fifo_mode)
			ret = aic_sdmc_idma_stop(host, &bbstate, data->flags);
	}

	udelay(100);

	return ret;
}

static void aic_sdmc_set_ext_clk_mux(struct aic_sdmc *host, u32 mux)
{
	u32 temp = sdmc_readl(host, SDMC_DLYCTRL);

	temp &= ~SDMC_DLYCTRL_EXT_CLK_MUX_MASK;
	switch (mux) {
	case 1:
		temp |= (u32)SDMC_DLYCTRL_EXT_CLK_MUX_1 <<
			SDMC_DLYCTRL_EXT_CLK_MUX_SHIFT;
		break;
	case 2:
		temp |= SDMC_DLYCTRL_EXT_CLK_MUX_2 <<
			SDMC_DLYCTRL_EXT_CLK_MUX_SHIFT;
		break;
	case 4:
	default:
		temp |= SDMC_DLYCTRL_EXT_CLK_MUX_4 <<
			SDMC_DLYCTRL_EXT_CLK_MUX_SHIFT;
		break;
	}

	sdmc_writel(host, SDMC_DLYCTRL, temp);
}

static u32 aic_sdmc_get_best_div(u32 sclk, u32 target_freq)
{
	u32 down, up, f_down, f_up;

	down = sclk / target_freq;
	up = DIV_ROUND_UP(sclk, target_freq);

	f_down = down == 0 ? sclk : sclk / down;
	f_up = up == 0 ? sclk : sclk / up;

	/* Select the closest div parameter */
	if ((f_down - target_freq) < (target_freq - f_up))
		return down;
	return up;
}

static int aic_sdmc_setup_bus(struct aic_sdmc *host, u32 freq)
{
	u32 mux, div, status;
	int timeout = SDMC_TIMEOUT;
	unsigned long sclk;
	u32 temp;

	if ((freq == host->clock) || (freq == 0))
		return 0;

	if (host->get_mmc_clk)
		sclk = host->get_mmc_clk(host, freq);
	else if (host->bus_hz)
		sclk = host->bus_hz;
	else {
		pr_err("%s: Didn't get source clock value.\n", __func__);
		return -EINVAL;
	}

	if (sclk == freq) {
		/* bypass mode */
		mux = 1;
		div = 0;
	} else {
		div = aic_sdmc_get_best_div(sclk, freq);
		if (div <= 4) {
			mux = DIV_ROUND_UP(div, 2);
		} else {
			if (div % 8)
				mux = 2;
			else
				mux = 4;
		}
		div /= mux * 2;
		if (div > SDMC_CLKCTRL_DIV_MAX)
			div = SDMC_CLKCTRL_DIV_MAX;
	}
	aic_sdmc_set_ext_clk_mux(host, mux);
	pr_info("SDMC%d Buswidth %d, sclk %ld KHz, req %d KHz, actual %ld KHz, div %d-%d\n",
		host->dev_index, host->mmc->bus_width, sclk / 1000,
		freq / 1000,
		div ? sclk / mux / div / 2 / 1000 : sclk / mux / 1000,
		mux, div);

	temp = sdmc_readl(host, SDMC_CLKCTRL);
	temp &= ~SDMC_CLKCTRL_EN;
	sdmc_writel(host, SDMC_CLKCTRL, temp);

	temp = sdmc_readl(host, SDMC_CLKCTRL);
	temp &= ~SDMC_CLKCTRL_DIV_MASK;
	temp |= (div << SDMC_CLKCTRL_DIV_SHIFT) & SDMC_CLKCTRL_DIV_MASK;
	sdmc_writel(host, SDMC_CLKCTRL, temp);

	sdmc_writel(host, SDMC_CMD, SDMC_CMD_PRV_DAT_WAIT |
			SDMC_CMD_UPD_CLK | SDMC_CMD_START);

	do {
		status = sdmc_readl(host, SDMC_CMD);
		if (timeout-- < 0) {
			pr_warn("%s: Timeout!\n", __func__);
			return -ETIMEDOUT;
		}
	} while (status & SDMC_CMD_START);

	temp = sdmc_readl(host, SDMC_CLKCTRL);
	temp |= SDMC_CLKCTRL_EN | SDMC_CLKCTRL_LOW_PWR;
	sdmc_writel(host, SDMC_CLKCTRL, temp);

	sdmc_writel(host, SDMC_CMD, SDMC_CMD_PRV_DAT_WAIT |
			SDMC_CMD_UPD_CLK | SDMC_CMD_START);

	timeout = SDMC_TIMEOUT;
	do {
		status = sdmc_readl(host, SDMC_CMD);
		if (timeout-- < 0) {
			pr_warn("%s: Timeout!\n", __func__);
			return -ETIMEDOUT;
		}
	} while (status & SDMC_CMD_START);

	host->clock = freq;

	return 0;
}

static int aic_sdmc_init(struct mmc *mmc)
{
	u32 temp;
	struct aic_sdmc *host = mmc->priv;

	if (host->board_init)
		host->board_init(host);

	if (aic_sdmc_reset(host, SDMC_HCTRL1_RESET_ALL)) {
		pr_err("%s() - Failed to reset!\n", __func__);
		return -EIO;
	}

	aic_sdmc_setup_bus(host, mmc->cfg->f_min);

	sdmc_writel(host, SDMC_OINTST, 0xFFFFFFFF);
	sdmc_writel(host, SDMC_INTEN, 0);

	sdmc_writel(host, SDMC_TTMC, 0xFFFFFFFF);

	sdmc_writel(host, SDMC_IDMAINTEN, 0);
	sdmc_writel(host, SDMC_PBUSCFG, 1);

	if (!host->fifoth_val) {
		u32 fifo_size;

		fifo_size = sdmc_readl(host, SDMC_FIFOCFG);
		fifo_size = ((fifo_size & RX_WMARK_MASK) >> RX_WMARK_SHIFT) + 1;
		host->fifoth_val = MSIZE(0x2) | RX_WMARK(fifo_size / 2 - 1) |
				TX_WMARK(fifo_size / 2);
	}
	sdmc_writel(host, SDMC_FIFOCFG, host->fifoth_val);

	temp = sdmc_readl(host, SDMC_DLYCTRL);
	temp &= ~SDMC_DLYCTRL_CLK_DRV_PHA_MASK;
	temp &= ~SDMC_DLYCTRL_CLK_SMP_PHA_MASK;
	temp |= host->driver_phase << SDMC_DLYCTRL_CLK_DRV_PHA_SHIFT |
		host->driver_delay << SDMC_DLYCTRL_CLK_DRV_DLY_SHIFT |
		host->sample_phase << SDMC_DLYCTRL_CLK_SMP_PHA_SHIFT |
		host->sample_delay << SDMC_DLYCTRL_CLK_SMP_DLY_SHIFT;
	sdmc_writel(host, SDMC_DLYCTRL, temp);

	temp = sdmc_readl(host, SDMC_CLKCTRL);
	temp &= ~SDMC_CLKCTRL_EN;
	sdmc_writel(host, SDMC_CLKCTRL, temp);
	return 0;
}

#ifdef CONFIG_DM_MMC
static int aic_sdmc_set_ios(struct udevice *dev)
{
	struct mmc *mmc = mmc_get_mmc_dev(dev);
#else
static int aic_sdmc_set_ios(struct mmc *mmc)
{
#endif
	u32 temp;
	struct aic_sdmc *host = (struct aic_sdmc *)mmc->priv;
	u32 ctype, regs;

	aic_sdmc_setup_bus(host, mmc->clock);
	switch (mmc->bus_width) {
	case 8:
		ctype = SDMC_HCTRL2_BW_8BIT;
		break;
	case 4:
		ctype = SDMC_HCTRL2_BW_4BIT;
		break;
	default:
		ctype = SDMC_HCTRL2_BW_1BIT;
		break;
	}

	temp = sdmc_readl(host, SDMC_HCTRL2);
	temp &= ~(0x3 << 28);
	temp |= ctype;
	sdmc_writel(host, SDMC_HCTRL2, temp);

	regs = sdmc_readl(host, SDMC_HCTRL2);
	if (mmc->ddr_mode)
		regs |= SDMC_DDR_MODE;
	else
		regs &= ~SDMC_DDR_MODE;

	sdmc_writel(host, SDMC_HCTRL2, regs);

	if (host->clksel)
		host->clksel(host);

#if CONFIG_IS_ENABLED(DM_REGULATOR)
	if (mmc->vqmmc_supply) {
		int ret;

		if (mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			regulator_set_value(mmc->vqmmc_supply, 1800000);
		else
			regulator_set_value(mmc->vqmmc_supply, 3300000);

		ret = regulator_set_enable_if_allowed(mmc->vqmmc_supply, true);
		if (ret)
			return ret;
	}
#endif

	return 0;
}

static int aic_sdmc_wait_dat0(struct udevice *dev, int state, int timeout_us)
{
	struct mmc *mmc = mmc_get_mmc_dev(dev);
	struct aic_sdmc *host = mmc->priv;
	bool wait_state = !!state;
	bool dat0_state;
	u32 val;

	timeout_us = DIV_ROUND_UP(timeout_us, 10); /* check every 10 us. */
	while (timeout_us--) {
		val = sdmc_readl(host, SDMC_CTRST);
		/* Value in register is inverted of dat0 */
		dat0_state = !((val & DAT0_STATUS_MSK) >> DAT0_STATUS_OFS);
		if (dat0_state == wait_state)
			return 0;
		udelay(10);
	}
	return -ETIMEDOUT;
}

#ifdef CONFIG_DM_MMC
const struct dm_mmc_ops aic_dmmmc_ops = {
	.send_cmd	= aic_sdmc_send_cmd,
	.set_ios	= aic_sdmc_set_ios,
	.wait_dat0	= aic_sdmc_wait_dat0,
};
#else
static const struct mmc_ops aic_mmc_ops = {
	.send_cmd	= aic_sdmc_send_cmd,
	.set_ios	= aic_sdmc_set_ios,
	.wait_dat0	= aic_sdmc_wait_dat0,
	.init		= aic_sdmc_init,
};
#endif

static uint aic_sdmc_get_mmc_clk(struct aic_sdmc *host, uint freq)
{
	struct udevice *dev = host->priv;
	struct aic_sdmc_priv *priv = dev_get_priv(dev);

#ifdef CONFIG_FPGA_BOARD_ARTINCHIP
	freq = 48000000;
#endif
	freq = clk_get_rate(&priv->clk);
	freq = freq / 2;
	return freq;
}

static int aic_sdmc_ofdata_to_platdata(struct udevice *dev)
{
#if !CONFIG_IS_ENABLED(OF_PLATDATA)
	struct aic_sdmc_priv *priv = dev_get_priv(dev);
	struct aic_sdmc_plat *plat = dev_get_plat(dev);
	struct aic_sdmc *host = &priv->host;
	int ret;

	host->name = dev->name;
	host->ioaddr = dev_read_addr_ptr(dev);

	ret = mmc_of_parse(dev, &plat->cfg);
	if (ret)
		return ret;

	host->buswidth = dev_read_u32_default(dev, "bus-width", 4);
	host->get_mmc_clk = aic_sdmc_get_mmc_clk;
	host->priv = dev;
	host->dev_index = dev->seq_;

	priv->fifo_depth = dev_read_u32_default(dev, "fifo-depth", 0);

	if (priv->fifo_depth < 0)
		return -EINVAL;
	priv->fifo_mode = dev_read_bool(dev, "fifo-mode");

	priv->f_max = plat->cfg.f_max;
	priv->f_min = CLOCK_MIN;
	if (!ret && priv->f_max < CLOCK_MIN)
		return -EINVAL;

	host->sample_phase = dev_read_u32_default(dev, "aic,sample-phase", 0);
	host->sample_delay = dev_read_u32_default(dev, "aic,sample-delay", 0);
	host->driver_phase = dev_read_u32_default(dev, "aic,driver-phase", 0);
	host->driver_delay = dev_read_u32_default(dev, "aic,driver-delay", 0);
#endif
	return 0;
}

static void aic_sdmc_get_platdata(struct udevice *dev)
{
#if CONFIG_IS_ENABLED(OF_PLATDATA)
	struct aic_sdmc_plat *plat = dev_get_platdata(dev);
	struct dtd_artinchip_aic_sdmc *dtplat = &plat->dtplat;
	struct aic_sdmc_priv *priv = dev_get_priv(dev);
	struct aic_sdmc *host = &priv->host;

	host->name = dev->name;
	host->ioaddr = map_sysmem(dtplat->reg[0], dtplat->reg[1]);
	host->buswidth = dtplat->bus_width;
	host->get_mmc_clk = aic_sdmc_get_mmc_clk;
	host->priv = dev;
	host->dev_index = 0;

	priv->fifo_depth = dtplat->fifo_depth;
	priv->fifo_mode = false;

	if (priv->fifo_depth < 0)
		priv->fifo_depth = 8;
	priv->f_max = dtplat->max_frequency;
	priv->f_min = CLOCK_MIN;
#endif
}

static int aic_sdmc_clk_gating(struct udevice *dev)
{
	int ret = 0;

	/* U-Boot always need to gating clock;
	 * SPL should to check the CONFIG_SPL_CLK_ARTINCHIP
	 */
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_CLK_ARTINCHIP)
	struct aic_sdmc_priv *priv = dev_get_priv(dev);

	ret = clk_get_by_index(dev, 0, &priv->clk);
	if (ret < 0)
		return ret;
	clk_disable(&priv->clk);

	ret = reset_get_by_index(dev, 0, &priv->reset);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get reset\n");
		return ret;
	}
	reset_assert(&priv->reset);

	ret = clk_enable(&priv->clk);
	if (ret) {
		dev_err(dev, "failed to enable clock (ret=%d)\n", ret);
		return ret;
	}
	ret = reset_deassert(&priv->reset);
	if (ret) {
		dev_err(dev, "failed to deassert reset\n");
		return ret;
	}
	clk_set_rate(&priv->clk, priv->f_max);
#endif
	return ret;
}

void aic_sdmc_setup_cfg(struct mmc_config *cfg, struct aic_sdmc *host,
			u32 max_clk, u32 min_clk)
{
	cfg->name = host->name;
#ifndef CONFIG_DM_MMC
	cfg->ops = &aic_mmc_ops;
#endif
	cfg->f_min = min_clk;
	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	cfg->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;
}

static int aic_sdmc_probe(struct udevice *dev)
{
	struct aic_sdmc_plat *plat = dev_get_plat(dev);
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct aic_sdmc_priv *priv = dev_get_priv(dev);
	struct aic_sdmc *host = &priv->host;
	int ret = 0;

	aic_sdmc_get_platdata(dev);

	ret = aic_sdmc_clk_gating(dev);
	if (ret)
		return ret;

	host->fifoth_val = MSIZE(2) | RX_WMARK(7) | TX_WMARK(8);
	host->fifo_mode = priv->fifo_mode;

	aic_sdmc_setup_cfg(&plat->cfg, host, priv->f_max, priv->f_min);

	host->mmc = &plat->mmc;
	host->mmc->priv = &priv->host;
	host->mmc->dev = dev;
	upriv->mmc = host->mmc;

	ret = aic_sdmc_init(mmc_get_mmc_dev(dev));
	sdmc_writel(host, SDMC_THRCTL,
		    (512 << SDMC_CARDTHR_SHIFT) | SDMC_CARDRDTHR_EN);

	return ret;
}

static int aic_sdmc_bind(struct udevice *dev)
{
	struct aic_sdmc_plat *plat = dev_get_plat(dev);

	return mmc_bind(dev, &plat->mmc, &plat->cfg);
}

static const struct udevice_id aic_sdmc_ids[] = {
	{ .compatible = "artinchip,aic-sdmc" },
	{ }
};

U_BOOT_DRIVER(aic_sdmc_drv) = {
	.name                     = "aic_sdmc",
	.id                       = UCLASS_MMC,
	.of_match                 = aic_sdmc_ids,
	.of_to_plat               = aic_sdmc_ofdata_to_platdata,
	.ops                      = &aic_dmmmc_ops,
	.bind                     = aic_sdmc_bind,
	.probe                    = aic_sdmc_probe,
	.priv_auto                = sizeof(struct aic_sdmc_priv),
	.plat_auto                = sizeof(struct aic_sdmc_plat),
};
