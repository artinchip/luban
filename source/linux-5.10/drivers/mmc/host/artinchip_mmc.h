/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2022 ArtInChip Inc.
 * Authors: matteo <mintao.duan@artinchip.com>
 */


#ifndef _ARTINCHIP_MMC_H_
#define _ARTINCHIP_MMC_H_

#include <linux/scatterlist.h>
#include <linux/mmc/core.h>
#include <linux/dmaengine.h>
#include <linux/reset.h>
#include <linux/interrupt.h>

enum artinchip_mmc_state {
	STATE_IDLE = 0,
	STATE_SENDING_CMD,
	STATE_SENDING_DATA,
	STATE_DATA_BUSY,
	STATE_SENDING_STOP,
	STATE_DATA_ERROR,
	STATE_SENDING_CMD11,
	STATE_WAITING_CMD11_DONE,
};

enum {
	EVENT_CMD_COMPLETE = 0,
	EVENT_XFER_COMPLETE,
	EVENT_DATA_COMPLETE,
	EVENT_DATA_ERROR,
};

enum artinchip_mmc_cookie {
	COOKIE_UNMAPPED,
	COOKIE_PRE_MAPPED,	/* mapped by pre_req() of aicmmc */
	COOKIE_MAPPED,		/* mapped by prepare_data() of aicmmc */
};

struct mmc_data;

enum data_width {
	DATA_WIDTH_16BIT = 1,
	DATA_WIDTH_32BIT,
	DATA_WIDTH_64BIT
};

/**
 * struct artinchip_mmc - MMC controller state shared between all slots
 * @lock: Spinlock protecting the queue and associated data.
 * @irq_lock: Spinlock protecting the INTMASK setting.
 * @regs: Pointer to MMIO registers.
 * @fifo_reg: Pointer to MMIO registers for data FIFO
 * @sg: Scatterlist entry currently being processed by PIO code, if any.
 * @sg_miter: PIO mapping scatterlist iterator.
 * @mrq: The request currently being processed on @slot,
 *	or NULL if the controller is idle.
 * @cmd: The command currently being sent to the card, or NULL.
 * @data: The data currently being transferred, or NULL if no data
 *	transfer is in progress.
 * @stop_abort: The command currently prepared for stopping transfer.
 * @prev_blksz: The former transfer blksz record.
 * @timing: Record of current ios timing.
 * @use_dma: Which DMA channel is in use for the current transfer, zero
 *	denotes PIO mode.
 * @using_dma: Whether DMA is in use for the current transfer.
 * @sg_dma: Bus address of DMA buffer.
 * @sg_cpu: Virtual address of DMA buffer.
 * @dma_ops: Pointer to platform-specific DMA callbacks.
 * @cmd_status: Snapshot of SR taken upon completion of the current
 * @ring_size: Buffer size for idma descriptors.
 *	command. Only valid when EVENT_CMD_COMPLETE is pending.
 * @phy_regs: physical address of controller's register map
 * @data_status: Snapshot of SR taken upon completion of the current
 *	data transfer. Only valid when EVENT_DATA_COMPLETE or
 *	EVENT_DATA_ERROR is pending.
 * @stop_cmdr: Value to be loaded into CMDR when the stop command is
 *	to be sent.
 * @dir_status: Direction of current transfer.
 * @tasklet: Tasklet running the request state machine.
 * @pending_events: Bitmask of events flagged by the interrupt handler
 *	to be processed by the tasklet.
 * @completed_events: Bitmask of events which the state machine has
 *	processed.
 * @state: Tasklet state.
 * @queue: List of slots waiting for access to the controller.
 * @sclk_rate: The rate of SDMC clk in Hz. It's the basis clk for SDMC.
 * @current_speed: Configured rate of the controller.
 * @fifoth_val: The value of FIFOTH register.
 * @verid: Denote Version ID.
 * @dev: Device associated with the MMC controller.
 * @pdata: Platform data associated with the MMC controller.
 * @drv_data: Driver specific data for identified variant of the controller
 * @priv: Implementation defined private data.
 * @hif_clk: Pointer to the interface of SDMC host.
 * @slot: Slots sharing this MMC controller.
 * @fifo_depth: depth of FIFO.
 * @data_addr_override: override fifo reg offset with this value.
 * @wm_aligned: force fifo watermark equal with data length in PIO mode.
 *	Set as true if alignment is needed.
 * @data_shift: log2 of FIFO item size.
 * @part_buf_start: Start index in part_buf.
 * @part_buf_count: Bytes of partial data in part_buf.
 * @part_buf: Simple buffer for partial fifo reads/writes.
 * @push_data: Pointer to FIFO push function.
 * @pull_data: Pointer to FIFO pull function.
 * @vqmmc_enabled: Status of vqmmc, should be true or false.
 * @irq_flags: The flags to be passed to request_irq.
 * @irq: The irq value to be passed to request_irq.
 * @cmd11_timer: Timer for SD3.0 voltage switch over scheme.
 * @cto_timer: Timer for broken command transfer over scheme.
 * @dto_timer: Timer for broken data transfer over scheme.
 *
 * Locking
 * =======
 *
 * @lock is a softirq-safe spinlock protecting @queue as well as
 * @slot, @mrq and @state. These must always be updated
 * at the same time while holding @lock.
 * The @mrq field of struct artinchip_mmc_slot is also protected by @lock,
 * and must always be written at the same time as the slot is added to
 * @queue.
 *
 * @irq_lock is an irq-safe spinlock protecting the INTMASK register
 * to allow the interrupt handler to modify it directly.  Held for only long
 * enough to read-modify-write INTMASK and no other locks are grabbed when
 * holding this one.
 *
 * @pending_events and @completed_events are accessed using atomic bit
 * operations, so they don't need any locking.
 *
 * None of the fields touched by the interrupt handler need any
 * locking. However, ordering is important: Before EVENT_DATA_ERROR or
 * EVENT_DATA_COMPLETE is set in @pending_events, all data-related
 * interrupts must be disabled and @data_status updated with a
 * snapshot of SR. Similarly, before EVENT_CMD_COMPLETE is set, the
 * CMDRDY interrupt must be disabled and @cmd_status updated with a
 * snapshot of SR, and before EVENT_XFER_COMPLETE can be set, the
 * bytes_xfered field of @data must be written. This is ensured by
 * using barriers.
 */
struct artinchip_mmc {
	spinlock_t		lock;
	spinlock_t		irq_lock;
	struct attribute_group	attrs;
	void __iomem		*regs;
	void __iomem		*fifo_reg;
	bool			wm_aligned;

	struct scatterlist	*sg;
	struct sg_mapping_iter	sg_miter;

	struct mmc_request	*mrq;
	struct mmc_command	*cmd;
	struct mmc_data		*data;
	struct mmc_command	stop_abort;
	unsigned int		prev_blksz;
	unsigned char		timing;

	/* DMA interface members*/
	bool			use_dma;
	bool			using_dma;

	dma_addr_t		sg_dma;
	void			*sg_cpu;
	const struct artinchip_mmc_dma_ops	*dma_ops;
	/* For idmac */
	unsigned int		ring_size;

	/* Registers's physical base address */
	resource_size_t		phy_regs;

	u32			cmd_status;
	u32			data_status;
	u32			stop_cmdr;
	u32			dir_status;
	struct tasklet_struct	tasklet;
	unsigned long		pending_events;
	unsigned long		completed_events;
	enum artinchip_mmc_state	state;
	struct list_head	queue;

	u32			sclk_rate;
	u32			current_speed;
	u32			fifoth_val;
	u16			verid;
	struct device		*dev;
	struct artinchip_mmc_board	*pdata;
	const struct artinchip_mmc_drv_data	*drv_data;
	void			*priv;
	struct clk		*hif_clk;
	struct reset_control	*reset;
	struct artinchip_mmc_slot	*slot;

	/* FIFO push and pull */
	int			fifo_depth;
	int			data_shift;
	u8			part_buf_start;
	u8			part_buf_count;
	enum data_width	data_width;
	union {
		u16		part_buf16;
		u32		part_buf32;
		u64		part_buf;
	};

	bool			vqmmc_enabled;
	unsigned long		irq_flags; /* IRQ flags */
	int			irq;

	struct timer_list       cmd11_timer;
	struct timer_list       cto_timer;
	struct timer_list       dto_timer;

	u32			sample_phase;
	u32			sample_delay;
	u32			driver_phase;
	u32			driver_delay;
	u32			power_gpio;
};

/* DMA ops for Internal/External DMAC interface */
struct artinchip_mmc_dma_ops {
	/* DMA Ops */
	int (*init)(struct artinchip_mmc *host);
	int (*start)(struct artinchip_mmc *host, unsigned int sg_len);
	void (*complete)(void *host);
	void (*stop)(struct artinchip_mmc *host);
	void (*cleanup)(struct artinchip_mmc *host);
	void (*exit)(struct artinchip_mmc *host);
};

struct dma_pdata;

/* Board platform data */
struct artinchip_mmc_board {
	u32 sclk_rate; /* SDMC clk rate */

	u32 caps;	/* Capabilities */
	u32 caps2;	/* More capabilities */
	u32 pm_caps;	/* PM capabilities */
	/*
	 * Override fifo depth. If 0, autodetect it from the FIFOTH register,
	 * but note that this may not be reliable after a bootloader has used
	 * it.
	 */
	unsigned int fifo_depth;

	/* delay in mS before detecting cards after interrupt */
	u32 detect_delay_ms;

	struct artinchip_mmc_dma_ops *dma_ops;
	struct dma_pdata *data;
};

#define ARTINCHIP_MMC_FREQ_MAX	200000000	/* unit: HZ */
#define ARTINCHIP_MMC_FREQ_MIN	100000		/* unit: HZ */

#define ARTINCHIP_MMC_SEND_STATUS	1
#define ARTINCHIP_MMC_RECV_STATUS	2
#define ARTINCHIP_MMC_DMA_THRESHOLD	16

/* SDMC controller register */
#define SDMC_BLKCNT		0x000
#define SDMC_BLKSIZ		0x004
#define SDMC_CMDARG		0x008
#define SDMC_CMD		0x00c
#define SDMC_RESP0		0x010
#define SDMC_RESP1		0x014
#define SDMC_RESP2		0x018
#define SDMC_RESP3		0x01c
#define SDMC_TTMC		0x020
#define SDMC_TCBC		0x024
#define SDMC_TFBC		0x028
#define SDMC_CTRST		0x02c
#define SDMC_HCTRL1		0x030
#define SDMC_CLKCTRL		0x034
#define SDMC_HCTRL2		0x038
#define SDMC_INTEN		0x03c
#define SDMC_INTST		0x040
#define SDMC_OINTST		0x044
#define SDMC_FIFOCFG		0x048
#define SDMC_HINFO		0x050
#define SDMC_CDET		0x054
#define SDMC_PBUSCFG		0x080
#define SDMC_IDMARCAP		0x084
#define SDMC_IDMASADDR		0x088
#define SDMC_IDMAST		0x08c
#define SDMC_IDMAINTEN		0x090
#define SDMC_IDMACDA		0x094
#define SDMC_IDMACBA		0x098
#define SDMC_CTC		0x100
#define SDMC_DLYCTRL		0x104
#define SDMC_EMCR		0x108
#define SDMC_VERID		0x118
#define SDMC_FIFO_DATA		0x200

/* Command configure register defines */
#define SDMC_CMD_START			BIT(31)
#define SDMC_CMD_USE_HOLD_REG		BIT(29)
#define SDMC_CMD_VOLT_SWITCH		BIT(28)
#define SDMC_CMD_UPD_CLK		BIT(21)
#define SDMC_CMD_CARDNUMB_MASK		GENMASK(20, 16)
#define SDMC_CMD_INIT			BIT(15)
#define SDMC_CMD_STOP			BIT(14)
#define SDMC_CMD_PRV_DAT_WAIT		BIT(13)
#define SDMC_CMD_SEND_STOP		BIT(12)
#define SDMC_CMD_TRANSFER_MODE		BIT(11)
#define SDMC_CMD_DAT_WR			BIT(10)
#define SDMC_CMD_DAT_EXP		BIT(9)
#define SDMC_CMD_RESP_CRC		BIT(8)
#define SDMC_CMD_RESP_LEN		BIT(7)
#define SDMC_CMD_RESP_EXP		BIT(6)
#define SDMC_CMD_INDX(n)		((n) & 0x1F)

/* Transfer timeout control register defines */
#define SDMC_TTMC_DATA_SHIFT		8
#define SDMC_TTMC_DATA_MASK		GENMASK(31, 8)
#define SDMC_TTMC_DATA(n)		((n) << SDMC_TTMC_DATA_SHIFT)
#define SDMC_TTMC_RESP_MASK		GENMASK(7, 0)
#define SDMC_TTMC_RESP(n)		((n) & SDMC_TTMC_RESP_MASK)

/* Controller status register defines */
#define SDMC_CTRST_DMA_REQ		BIT(31)
#define SDMC_CTRST_FCNT(x)		(((x) >> 17) & 0x1FFF)
#define SDMC_CTRST_BUSY			BIT(9)

/* Host control 1 register defines */
#define SDMC_HCTRL1_USE_IDMAC		BIT(25)
#define SDMC_HCTRL1_ABRT_READ_DATA	BIT(8)
#define SDMC_HCTRL1_SEND_IRQ_RESP	BIT(7)
#define SDMC_HCTRL1_READ_WAIT		BIT(6)
#define SDMC_HCTRL1_DMA_EN		BIT(5)
#define SDMC_HCTRL1_INT_EN		BIT(4)
#define SDMC_HCTRL1_CARD_RESET		BIT(3)
#define SDMC_HCTRL1_DMA_RESET		BIT(2)
#define SDMC_HCTRL1_FIFO_RESET		BIT(1)
#define SDMC_HCTRL1_RESET		BIT(0)
#define SDMC_HCTRL1_RESET_ALL \
	(SDMC_HCTRL1_RESET | SDMC_HCTRL1_FIFO_RESET | SDMC_HCTRL1_DMA_RESET)

/* Clock control register defines */
#define SDMC_CLKCTRL_LOW_PWR		BIT(16)
#define SDMC_CLKCTRL_DIV_SHIFT		8
#define SDMC_CLKCTRL_DIV_MASK		GENMASK(15, 8)
#define SDMC_CLKCTRL_DIV_MAX \
		(SDMC_CLKCTRL_DIV_MASK >> SDMC_CLKCTRL_DIV_SHIFT)
#define SDMC_GET_CLK_DIV(x) \
		((((x) & SDMC_CLKCTRL_DIV_MASK) >> SDMC_CLKCTRL_DIV_SHIFT) * 2)
#define SDMC_CLKCTRL_EN			BIT(0)

/* Host control 2 register defines */
#define SDMC_HCTRL2_BW_8BIT		BIT(29)
#define SDMC_HCTRL2_BW_4BIT		BIT(28)
#define SDMC_HCTRL2_BW_1BIT		0
#define SDMC_HCTRL2_DDR_MODE		BIT(16)
#define SDMC_HCTRL2_VOLT_18V		BIT(0)

/* Interrupt status & enable register defines */
#define SDMC_INT_SDIO			BIT(16)
#define SDMC_INT_EBE			BIT(15)
#define SDMC_INT_AUTO_CMD_DONE		BIT(14)
#define SDMC_INT_SBE			BIT(13)
#define SDMC_INT_HLE			BIT(12)
#define SDMC_INT_FRUN			BIT(11)
#define SDMC_INT_HTO			BIT(10)
#define SDMC_INT_VOLT_SWITCH		BIT(10) /* overloads bit 10! */
#define SDMC_INT_DRTO			BIT(9)
#define SDMC_INT_RTO			BIT(8)
#define SDMC_INT_DCRC			BIT(7)
#define SDMC_INT_RCRC			BIT(6)
#define SDMC_INT_RXDR			BIT(5)
#define SDMC_INT_TXDR			BIT(4)
#define SDMC_INT_DAT_DONE		BIT(3)
#define SDMC_INT_CMD_DONE		BIT(2)
#define SDMC_INT_RESP_ERR		BIT(1)
#define SDMC_INT_CD			BIT(0)
#define SDMC_INT_ERROR \
		(SDMC_INT_EBE | SDMC_INT_SBE | SDMC_INT_HLE | \
		 SDMC_INT_FRUN | SDMC_INT_HTO | SDMC_INT_DRTO | SDMC_INT_RTO \
		 SDMC_INT_DCRC | SDMC_INT_RCRC | SDMC_INT_RESP_ERR)
#define SDMC_DAT_ERROR_FLAGS	(SDMC_INT_EBE | SDMC_INT_SBE | SDMC_INT_HLE | \
				 SDMC_INT_HTO | SDMC_INT_DRTO | SDMC_INT_DCRC)
#define SDMC_CMD_ERROR_FLAGS	(SDMC_INT_RTO | SDMC_INT_RCRC | \
				 SDMC_INT_RESP_ERR | SDMC_INT_HLE)
#define SDMC_ERROR_FLAGS	(SDMC_DAT_ERROR_FLAGS | SDMC_CMD_ERROR_FLAGS)

/* FIFO configuration register defines */
#define SDMC_FIFOCFG_SET_THD(m, r, t) \
		(((m) & 0x7) << 28 | ((r) & 0xFFF) << 16 | ((t) & 0xFFF))

/* Hardware information register defines */
#define SDMC_HINFO_IF_IDMA		(0x0)
#define SDMC_HINFO_IF_AICDMA		(0x1)
#define SDMC_HINFO_IF_GDMA		(0x2)
#define SDMC_HINFO_IF_NODMA		(0x3)
#define SDMC_HINFO_TRANS_MODE(x)	(((x) >> 16) & 0x3)
#define SDMC_HINFO_HDATA_WIDTH(x)	(((x) >> 7) & 0x7)
#define SDMC_HINFO_ADDR_CONFIG(x)	(((x) >> 27) & 0x1)

/* Card Detect Configuration register defines */
#define SDMC_CDET_ABSENT		BIT(24)

/* Peripheral bus configuration register defines */
#define SDMC_PBUSCFG_IDMAC_EN		BIT(7)
#define SDMC_PBUSCFG_IDMAC_FB		BIT(1)
#define SDMC_PBUSCFG_IDMAC_SWR		BIT(0)

/* Internal DMAC interrupt defines */
#define SDMC_IDMAC_INT_AIS		BIT(9)
#define SDMC_IDMAC_INT_NIS		BIT(8)
#define SDMC_IDMAC_INT_CES		BIT(5)
#define SDMC_IDMAC_INT_DU		BIT(4)
#define SDMC_IDMAC_INT_FBE		BIT(2)
#define SDMC_IDMAC_INT_RI		BIT(1)
#define SDMC_IDMAC_INT_TI		BIT(0)
#define SDMC_IDMAC_INT_MASK	(SDMC_IDMAC_INT_AIS | SDMC_IDMAC_INT_NIS | \
				 SDMC_IDMAC_INT_CES | SDMC_IDMAC_INT_DU | \
				 SDMC_IDMAC_INT_FBE | SDMC_IDMAC_INT_RI | \
				 SDMC_IDMAC_INT_TI)

/* Card threshold control register defines */
#define SDMC_CTC_THD_SHIFT		16
#define SDMC_CTC_THD_MASK		GENMASK(27, 16)
#define SDMC_CTC_SET_THD(v, x) \
		(((v) & SDMC_CTC_THD_MASK) << SDMC_CTC_THD_SHIFT | (x))
#define SDMC_CTC_WR_THD_EN		BIT(2)
#define SDMC_CTC_RD_THD_EN		BIT(0)

/* DDR register defines */
#define SDMC_DDR_HS400_ENABLE		BIT(31)
/* Enable shift register defines */
#define SDMC_ENABLE_PHASE		BIT(0)

#define SDMC_DLYCTRL_EXT_CLK_MUX_SHIFT	30
#define SDMC_DLYCTRL_EXT_CLK_MUX_MASK	GENMASK(31, 30)
#define SDMC_DLYCTRL_EXT_CLK_MUX_1	2
#define SDMC_DLYCTRL_EXT_CLK_MUX_2	1
#define SDMC_DLYCTRL_EXT_CLK_MUX_4	0
#define SDMC_DLYCTRL_CLK_DRV_PHA_MASK	GENMASK(29, 28)
#define SDMC_DLYCTRL_CLK_DRV_PHA_SHIFT	28
#define SDMC_DLYCTRL_CLK_DRV_PHA_0	0
#define SDMC_DLYCTRL_CLK_DRV_PHA_90	1
#define SDMC_DLYCTRL_CLK_DRV_PHA_180	2
#define SDMC_DLYCTRL_CLK_DRV_PHA_270	3
#define SDMC_DLYCTRL_CLK_DRV_DLY_MASK	GENMASK(27, 23)
#define SDMC_DLYCTRL_CLK_DRV_DLY_SHIFT	23
#define SDMC_DLYCTRL_CLK_SMP_PHA_MASK	GENMASK(22, 21)
#define SDMC_DLYCTRL_CLK_SMP_PHA_SHIFT	21
#define SDMC_DLYCTRL_CLK_SMP_PHA_0	0
#define SDMC_DLYCTRL_CLK_SMP_PHA_90	1
#define SDMC_DLYCTRL_CLK_SMP_PHA_180	2
#define SDMC_DLYCTRL_CLK_SMP_PHA_270	3
#define SDMC_DLYCTRL_CLK_DRV_DLY_MAX \
	(SDMC_DLYCTRL_CLK_DRV_DLY_MASK >> SDMC_DLYCTRL_CLK_DRV_DLY_SHIFT)
#define SDMC_DLYCTRL_CLK_SMP_DLY_MASK	GENMASK(20, 16)
#define SDMC_DLYCTRL_CLK_SMP_DLY_SHIFT	16
#define SDMC_DLYCTRL_CLK_SMP_DLY_MAX \
	(SDMC_DLYCTRL_CLK_SMP_DLY_MASK >> SDMC_DLYCTRL_CLK_SMP_DLY_SHIFT)
/* CLKSEL register defines */
#define SDMMC_DLYCTRL_CCLK_DRIVER(x)	(((x) & 3) << SDMC_DLYCTRL_CLK_DRV_PHA_SHIFT)
#define SDMMC_DLYCTRL_CCLK_UP_DRIVER(x, y)	(((x) & ~SDMMC_DLYCTRL_CCLK_DRIVER(3)) |\
					 SDMMC_DLYCTRL_CCLK_DRIVER(y))

/* Version ID register define */
#define SDMC_VERID_GET(x)		((x) & 0xFFFF)

/* FIFO register access macros. These should not change the data endian-ness
 * as they are written to memory to be dealt with by the upper layers
 */
#define mci_fifo_readw(__reg)	__raw_readw(__reg)
#define mci_fifo_readl(__reg)	__raw_readl(__reg)
#define mci_fifo_readq(__reg)	__raw_readq(__reg)

#define mci_fifo_writew(__value, __reg)	__raw_writew(__reg, __value)
#define mci_fifo_writel(__value, __reg)	__raw_writel(__reg, __value)
#define mci_fifo_writeq(__value, __reg)	__raw_writeq(__reg, __value)

/* Register access macros */
#define mci_readl(dev, reg)			\
	readl_relaxed((dev)->regs + reg)
#define mci_writel(dev, reg, value)			\
	writel_relaxed((value), (dev)->regs + reg)

/* 16-bit FIFO access macros */
#define mci_readw(dev, reg)			\
	readw_relaxed((dev)->regs + reg)
#define mci_writew(dev, reg, value)			\
	writew_relaxed((value), (dev)->regs + reg)

/* 64-bit FIFO access macros */
#ifdef readq
#define mci_readq(dev, reg)			\
	readq_relaxed((dev)->regs + reg)
#define mci_writeq(dev, reg, value)			\
	writeq_relaxed((value), (dev)->regs + reg)
#else
/*
 * Dummy readq implementation for architectures that don't define it.
 *
 * We would assume that none of these architectures would configure
 * the IP block with a 64bit FIFO width, so this code will never be
 * executed on those machines. Defining these macros here keeps the
 * rest of the code free from ifdefs.
 */
#define mci_readq(dev, reg)			\
	(*(volatile u64 __force *)((dev)->regs + reg))
#define mci_writeq(dev, reg, value)			\
	(*(volatile u64 __force *)((dev)->regs + reg) = (value))

#define __raw_writeq(__value, __reg) \
	(*(volatile u64 __force *)(__reg) = (__value))
#define __raw_readq(__reg) (*(volatile u64 __force *)(__reg))
#endif

extern int artinchip_mmc_probe(struct artinchip_mmc *host);
extern void artinchip_mmc_remove(struct artinchip_mmc *host);
#ifdef CONFIG_PM
extern int artinchip_mmc_runtime_suspend(struct device *device);
extern int artinchip_mmc_runtime_resume(struct device *device);
#endif

/**
 * struct artinchip_mmc_slot - MMC slot state
 * @mmc: The mmc_host representing this slot.
 * @host: The MMC controller this slot is using.
 * @ctype: Card type for this slot.
 * @mrq: mmc_request currently being processed or waiting to be
 *	processed, or NULL when the slot is idle.
 * @queue_node: List node for placing this node in the @queue list of
 *	&struct artinchip_mmc.
 * @clock: Clock rate configured by set_ios(). Protected by host->lock.
 * @__clk_old: The last clock value that was requested from core.
 *	Keeping track of this helps us to avoid spamming the console.
 * @flags: Random state bits associated with the slot.
 * @id: Number of this slot.
 */
struct artinchip_mmc_slot {
	struct mmc_host		*mmc;
	struct artinchip_mmc		*host;

	u32			ctype;

	struct mmc_request	*mrq;
	struct list_head	queue_node;

	unsigned int		clock;
	unsigned int		__clk_old;

	unsigned long		flags;
#define ARTINCHIP_MMC_CARD_PRESENT	0
#define ARTINCHIP_MMC_CARD_NEED_INIT	1
#define ARTINCHIP_MMC_CARD_NO_LOW_PWR	2
#define ARTINCHIP_MMC_CARD_NO_USE_HOLD 3
#define ARTINCHIP_MMC_CARD_NEEDS_POLL	4
};

/**
 * artinchip_mmc driver data - aic-shmc implementation specific driver data.
 * @caps: mmc subsystem specified capabilities of the controller(s).
 * @num_caps: number of capabilities specified by @caps.
 * @init: early implementation specific initialization.
 * @set_ios: handle bus specific extensions.
 * @parse_dt: parse implementation specific device tree properties.
 * @execute_tuning: implementation specific tuning procedure.
 *
 * Provide controller implementation specific extensions. The usage of this
 * data structure is fully optional and usage of each member in this structure
 * is optional as well.
 */
struct artinchip_mmc_drv_data {
	unsigned long	*caps;
	u32		num_caps;
	int		(*init)(struct artinchip_mmc *host);
	int		(*parse_dt)(struct artinchip_mmc *host);
	int		(*execute_tuning)(struct artinchip_mmc_slot *slot, u32 opcode);
	int		(*prepare_hs400_tuning)(struct artinchip_mmc *host,
						struct mmc_ios *ios);
	int		(*switch_voltage)(struct mmc_host *mmc,
					  struct mmc_ios *ios);
};
#endif /* _ARTINCHIP_MMC_H_ */
