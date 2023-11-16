/* SPDX-License-Identifier: GPL-2.0-only
 *
 * I2C driver of ArtInChip SoC
 *
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 * Authors:  dwj <weijie.ding@artinchip.com>
 */

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/completion.h>
#include <linux/i2c.h>

#define I2C_CTL					0x00
#define I2C_TAR					0x04
#define I2C_SAR					0x08
#define I2C_ACK_GEN_CALL			0x0C
#define I2C_DATA_CMD				0x10
#define I2C_SS_SCL_HCNT				0x20
#define I2C_SS_SCL_LCNT				0x24
#define I2C_FS_SCL_HCNT				0x28
#define I2C_FS_SCL_LCNT				0x2C
#define I2C_SDA_HOLD				0x30
#define I2C_SDA_SETUP				0x34
#define I2C_INTR_MASK				0x38
#define I2C_INTR_CLR				0x3C
#define I2C_INTR_STAT				0x40
#define I2C_ENABLE				0x48
#define I2C_ENABLE_STATUS			0x4C
#define I2C_STATUS				0x50
#define I2C_TX_ABRT_SOURCE			0x54
#define I2C_RX_TL				0x90
#define I2C_TX_TL				0x94
#define I2C_TXFLR				0x98
#define I2C_RXFLR				0x9C
#define I2C_SCL_STUCK_TIMEOUT			0xA0
#define I2C_SDA_STUCK_TIMEOUT			0xA4
#define I2C_FS_SPIKELEN				0xB0
#define I2C_VERSION				0xFC

#define I2C_CTL_10BIT_SELECT_MASTER		BIT(2)
#define I2C_CTL_10BIT_SELECT_SLAVE		BIT(3)
#define I2C_CTL_SPEED_MODE_SELECT_MASK		GENMASK(4, 5)
#define I2C_CTL_SPEED_MODE_SS			(0x1 << 4)
#define I2C_CTL_SPEED_MODE_FS			(0x2 << 4)
#define I2C_CTL_RESTART_ENABLE			BIT(6)
#define I2C_CTL_STOP_DET_IFADDR			BIT(7)
#define I2C_CTL_TX_EMPTY_CTL			BIT(8)
#define I2C_CTL_RX_FIFO_FULL_HLD		BIT(9)
#define I2C_CTL_BUS_CLEAR_FEATURE		BIT(10)

#define I2C_TAR_ADDR				GENMASK(0, 9)
#define I2C_TAR_START_BYTE			BIT(10)
#define I2C_TAR_GEN_CALL_CTL			BIT(11)

#define I2C_DATA_CMD_DAT			GENMASK(0, 7)
#define I2C_DATA_CMD_READ			BIT(8)
#define I2C_DATA_CMD_STOP			BIT(9)
#define I2C_DATA_CMD_RESTART			BIT(10)

#define I2C_SDA_TX_HOLD				GENMASK(0, 15)
#define I2C_SDA_RX_HOLD				GENMASK(16, 23)

#define I2C_INTR_RX_UNDER			BIT(0)
#define I2C_INTR_RX_OVER			BIT(1)
#define I2C_INTR_RX_FULL			BIT(2)
#define I2C_INTR_TX_OVER			BIT(3)
#define I2C_INTR_TX_EMPTY			BIT(4)
#define I2C_INTR_RD_REQ				BIT(5)
#define I2C_INTR_TX_ABRT			BIT(6)
#define I2C_INTR_RX_DONE			BIT(7)
#define I2C_INTR_ACTIVITY			BIT(8)
#define I2C_INTR_STOP_DET			BIT(9)
#define I2C_INTR_START_DET			BIT(10)
#define I2C_INTR_GEN_CALL			BIT(11)
#define I2C_INTR_MASTER_ON_HOLD			BIT(13)
#define I2C_INTR_SCL_STUCK_AT_LOW		BIT(14)

#define I2C_ENABLE_BIT				BIT(0)
#define I2C_ENABLE_ABORT			BIT(1)
#define I2C_TX_CMD_BLOCK			BIT(2)
#define I2C_SDA_STUCK_RECOVERY_ENABLE		BIT(3)

#define I2C_STATUS_ACTIVITY			BIT(0)

#define ABRT_7BIT_ADDR_NOACK			0
#define	ABRT_10BIT_ADDR1_NOACK			1
#define ABRT_10BIT_ADDR2_NOACK			2
#define ABRT_TXDATA_NOACK			3
#define ABRT_GCALL_NOACK			4
#define ABRT_GCALL_READ				5
#define ABRT_SBYTE_ACKDET			7
#define ABRT_SBYTE_NORSTRT			9
#define ABRT_10BIT_RD_NORSTRT			10
#define ABRT_MASTER_DIS				11
#define ABRT_LOST				12
#define ABRT_SLVFLUSH_TXFIFO			13
#define ABRT_SLV_ARBLOST			14
#define ABRT_SLVRD_INTX				15
#define ABRT_USER_ABRT				16
#define ABRT_SDA_STUCK_AT_LOW			17

#define I2C_ABRT_7BIT_ADDR_NOACK		BIT(0)
#define	I2C_ABRT_10BIT_ADDR1_NOACK		BIT(1)
#define I2C_ABRT_10BIT_ADDR2_NOACK		BIT(2)
#define I2C_ABRT_TXDATA_NOACK			BIT(3)
#define I2C_ABRT_GCALL_NOACK			BIT(4)
#define I2C_ABRT_GCALL_READ			BIT(5)
#define I2C_ABRT_SBYTE_ACKDET			BIT(7)
#define I2C_ABRT_SBYTE_NORSTRT			BIT(9)
#define I2C_ABRT_10BIT_RD_NORSTRT		BIT(10)
#define I2C_ABRT_MASTER_DIS			BIT(11)
#define I2C_ABRT_LOST				BIT(12)
#define I2C_ABRT_SLVFLUSH_TXFIFO		BIT(13)
#define I2C_ABRT_SLV_ARBLOST			BIT(14)
#define I2C_ABRT_SLVRD_INTX			BIT(15)
#define I2C_ABRT_USER_ABRT			BIT(16)
#define I2C_ABRT_SDA_STUCK_AT_LOW		BIT(17)

#define I2C_TX_ABRT_NOACK			(I2C_ABRT_7BIT_ADDR_NOACK |\
						 I2C_ABRT_10BIT_ADDR1_NOACK |\
						 I2C_ABRT_10BIT_ADDR2_NOACK |\
						 I2C_ABRT_TXDATA_NOACK |\
						 I2C_ABRT_GCALL_NOACK)

#define I2C_ENABLE_MASTER_DISABLE_SLAVE		(0x3)
#define I2C_ENABLE_SLAVE_DISABLE_MASTER		(0)

#define I2C_FIFO_DEPTH				8
#define I2C_TXFIFO_THRESHOLD			(I2C_FIFO_DEPTH / 2 - 1)
#define I2C_RXFIFO_THRESHOLD			0

#define I2C_INTR_DEFAULT_MASK			(I2C_INTR_RX_FULL |\
						 I2C_INTR_TX_ABRT)

#define I2C_INTR_MASTER_MASK			(I2C_INTR_DEFAULT_MASK |\
						 I2C_INTR_RX_OVER |\
						 I2C_INTR_RX_UNDER |\
						 I2C_INTR_TX_OVER |\
						 I2C_INTR_TX_EMPTY |\
						 I2C_INTR_STOP_DET)

#define I2C_INTR_SLAVE_MASK			(I2C_INTR_DEFAULT_MASK |\
						 I2C_INTR_START_DET |\
						 I2C_INTR_RX_DONE |\
						 I2C_INTR_RX_UNDER |\
						 I2C_INTR_RD_REQ)

#define I2C_INTR_ERROR_TX_RX			0x0001
#define I2C_INTR_ERROR_ABRT			0x0002

enum aic_i2c_speed {
	I2C_SPEED_STD		= 1,
	I2C_SPEED_FAST		= 2,
};

/*
 * Indicate one message status
 */

enum aic_msg_status {
	MSG_IDLE		= 0,
	MSG_IN_PROCESS		= 1,
};

struct aic_i2c_dev {
	struct device *dev;
	void __iomem *base;
	struct i2c_adapter adap;
	struct completion cmd_complete;
	struct clk *clk;
	struct reset_control *rst;
	int irq;
	enum aic_i2c_speed i2c_speed;
	u16 scl_hcnt;
	u16 scl_lcnt;
	u32 abort_source;
	struct i2c_msg *msg;
	enum aic_msg_status msg_status;
	int buf_write_idx;
	int buf_read_idx;
	bool is_first_message;
	bool is_last_message;
	int msg_err;
	struct i2c_timings timings;
	u32 master_cfg;
	u32 slave_cfg;
	struct i2c_client *slave;
};

u32 i2c_readl(struct aic_i2c_dev *i2c_dev, int offset);
void i2c_writel(struct aic_i2c_dev *i2c_dev, u32 val, int offset);
void i2c_disable_interrupts(struct aic_i2c_dev *i2c_dev);
void i2c_enable(struct aic_i2c_dev *i2c_dev);
void i2c_disable(struct aic_i2c_dev *i2c_dev);
int i2c_scl_cnt(u32 ic_clk, enum aic_i2c_speed aic_speed,
			u16 *hcnt, u16 *lcnt);
int i2c_handle_tx_abort(struct aic_i2c_dev *i2c_dev);
int i2c_wait_bus_free(struct aic_i2c_dev *i2c_dev);


extern int i2c_probe_master(struct aic_i2c_dev *i2c_dev);
#if IS_ENABLED(CONFIG_I2C_ARTINCHIP_SLAVE)
extern int i2c_probe_slave(struct aic_i2c_dev *i2c_dev);
#else
static inline int i2c_probe_slave(struct aic_i2c_dev *i2c_dev)
{
	return -EINVAL;
}
#endif

