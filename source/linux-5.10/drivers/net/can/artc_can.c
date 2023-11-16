// SPDX-License-Identifier: GPL-2.0-only
/* CAN bus controller driver for Artc SOC
 */
#include <linux/netdevice.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/can/error.h>
#include <linux/can/led.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#define DRV_NAME "artc_can"

/* Register address */
#define ARTC_CAN_MODE_REG			0x0000
#define ARTC_CAN_MCR_REG			0x0004
#define ARTC_CAN_STAT_REG			0x0008
#define ARTC_CAN_INTR_REG			0x000C
#define ARTC_CAN_INTEN_REG			0x0010
#define ARTC_CAN_BTR0_REG			0x0018
#define ARTC_CAN_BTR1_REG			0x001C
#define ARTC_CAN_ARBLOST_REG			0x002C
#define ARTC_CAN_ERRCODE_REG			0x0030
#define ARTC_CAN_ERRWT_REG			0x0034
#define ARTC_CAN_RXERR_REG			0x0038
#define ARTC_CAN_TXERR_REG			0x003C
#define ARTC_CAN_BUF0_REG			0x0040
#define ARTC_CAN_BUF1_REG			0x0044
#define ARTC_CAN_BUF2_REG			0x0048
#define ARTC_CAN_BUF3_REG			0x004C
#define ARTC_CAN_BUF4_REG			0x0050
#define ARTC_CAN_BUF5_REG			0x0054
#define ARTC_CAN_RXCODE0_REG			0x0040
#define ARTC_CAN_RXCODE1_REG			0x0044
#define ARTC_CAN_RXCODE2_REG			0x0048
#define ARTC_CAN_RXCODE3_REG			0x004C
#define ARTC_CAN_RXMASK0_REG			0x0050
#define ARTC_CAN_RXMASK1_REG			0x0054
#define ARTC_CAN_RXMASK2_REG			0x0058
#define ARTC_CAN_RXMASK3_REG			0x005C
#define ARTC_CAN_RXC_REG			0x0074
#define ARTC_CAN_RSADDR_REG			0x0078
#define ARTC_CAN_RXFIFO_REG			0x0080
#define ARTC_CAN_TXBRO_REG			0x0180
#define ARTC_CAN_VERSION_REG			0x0FFC

/* Register bit filed */
/* Mode register */
#define ARTC_CAN_MODE_SLEEP			BIT(4)
#define ARTC_CAN_MODE_WAKEUP			(0 << 4)
#define ARTC_CAN_MODE_FILTER_SINGLE		BIT(3)
#define ARTC_CAN_MODE_FILTER_DUAL		(0 << 3)
#define ARTC_CAN_MODE_SELFTEST			BIT(2)
#define ARTC_CAN_MODE_LISTEN			BIT(1)
#define ARTC_CAN_MODE_RST			BIT(0)

/* Control reg, write only */
#define ARTC_CAN_MCR_SELFREQ			BIT(4)
#define ARTC_CAN_MCR_CLR_OVF			BIT(3)
#define ARTC_CAN_MCR_RXB_REL			BIT(2)
#define ARTC_CAN_MCR_ABORTREQ			BIT(1)
#define ARTC_CAN_MCR_TXREQ			BIT(0)

/* Status reg, read only */
#define ARTC_CAN_STAT_BUS			BIT(7)
#define ARTC_CAN_STAT_ERR			BIT(6)
#define ARTC_CAN_STAT_TX			BIT(5)
#define ARTC_CAN_STAT_RX			BIT(4)
#define ARTC_CAN_STAT_TXC			BIT(3)
#define ARTC_CAN_STAT_TXB			BIT(2)
#define ARTC_CAN_STAT_OVF			BIT(1)
#define ARTC_CAN_STAT_RXB			BIT(0)

/* interrupt flag reg */
#define ARTC_CAN_INTR_ERRB			BIT(7)
#define ARTC_CAN_INTR_ARBLOST			BIT(6)
#define ARTC_CAN_INTR_ERRP			BIT(5)
#define ARTC_CAN_INTR_WAKEUP			BIT(4)
#define ARTC_CAN_INTR_OVF			BIT(3)
#define ARTC_CAN_INTR_ERRW			BIT(2)
#define ARTC_CAN_INTR_TX			BIT(1)
#define ARTC_CAN_INTR_RX			BIT(0)

/* interrupt enable reg */
#define ARTC_CAN_INTEN_ERRB			BIT(7)
#define ARTC_CAN_INTEN_ARBLOST			BIT(6)
#define ARTC_CAN_INTEN_ERRP			BIT(5)
#define ARTC_CAN_INTEN_WAKEUP			BIT(4)
#define ARTC_CAN_INTEN_OVF			BIT(3)
#define ARTC_CAN_INTEN_ERRW			BIT(2)
#define ARTC_CAN_INTEN_TXI			BIT(1)
#define ARTC_CAN_INTEN_RXI			BIT(0)

/* btr0 reg */
#define ARTC_CAN_BTR0_SJW_MASK			GENMASK(7, 6)
#define ARTC_CAN_BTR0_BRP_MASK			(0x3F)

/* btr1 reg */
#define ARTC_CAN_BTR1_SAM_MASK			BIT(7)
#define ARTC_CAN_BTR1_TS2_MASK			GENMASK(6, 4)
#define ARTC_CAN_BTR1_TS1_MASK			(0xF)

/* error code reg */
#define ARTC_CAN_ERRCODE_ERRTYPE_MASK		GENMASK(7, 6)
#define ARTC_CAN_ERRCODE_ERRTYPE_BIT		(0x0 << 6)
#define ARTC_CAN_ERRCODE_ERRTYPE_FORMAT		(0x1 << 6)
#define ARTC_CAN_ERRCODE_ERRTYPE_STUFF		(0x2 << 6)
#define ARTC_CAN_ERRCODE_ERRTYPE_OTHER		(0x3 << 6)
#define ARTC_CAN_ERRCODE_DIR			BIT(5)
#define ARTC_CAN_ERRCODE_SEGCODE_MASK		(0x1f)

/* buf0 reg */
#define ARTC_CAN_BUF0_MSG_EFF_FLAG		BIT(7)
#define ARTC_CAN_BUF0_MSG_RTR_FLAG		BIT(6)

/* max number of interrupts handled in ISR */
#define ARTC_CAN_MAX_IRQ			20
#define ARTC_CAN_MODE_MAX_RETRIES		100

struct artc_priv {
	struct can_priv can;
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct reset_control *rst;
	/* lock for concurrent mcr register writes */
	spinlock_t cmdreg_lock;
};

static const struct can_bittiming_const artccan_bittiming_const = {
	.name = DRV_NAME,
	.tseg1_min = 1,
	.tseg1_max = 16,
	.tseg2_min = 1,
	.tseg2_max = 8,
	.sjw_max = 4,
	.brp_min = 1,
	.brp_max = 64,
	.brp_inc = 1,
};

static int set_normal_mode(struct net_device *dev)
{
	struct artc_priv *priv = netdev_priv(dev);
	int retry = ARTC_CAN_MODE_MAX_RETRIES;
	u32 mod_reg_val = 0;

	mod_reg_val = readl(priv->base + ARTC_CAN_MODE_REG);
	while ((mod_reg_val & ARTC_CAN_MODE_RST) && retry--) {
		mod_reg_val &= ~ARTC_CAN_MODE_RST;
		writel(mod_reg_val, priv->base + ARTC_CAN_MODE_REG);
		udelay(10);
		mod_reg_val = readl(priv->base + ARTC_CAN_MODE_REG);
	}

	if (mod_reg_val & ARTC_CAN_MODE_RST) {
		netdev_err(dev, "setting ARTC can into normal mode failed\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int set_reset_mode(struct net_device *dev)
{
	struct artc_priv *priv = netdev_priv(dev);
	int retry = ARTC_CAN_MODE_MAX_RETRIES;
	u32 mod_reg_val = 0;

	mod_reg_val = readl(priv->base + ARTC_CAN_MODE_REG);
	while (!(mod_reg_val & ARTC_CAN_MODE_RST) && retry--) {
		mod_reg_val = ARTC_CAN_MODE_RST;
		writel(mod_reg_val, priv->base + ARTC_CAN_MODE_REG);
		udelay(10);
		mod_reg_val = readl(priv->base + ARTC_CAN_MODE_REG);
	}

	if (!(mod_reg_val & ARTC_CAN_MODE_RST)) {
		netdev_err(dev, "setting ARTC can into reset mode failed\n");
		return -ETIMEDOUT;
	}

	return 0;
}

/* Configure CAN bittiming, this function is called in reset mode only */
static int artc_can_set_bittiming(struct net_device *dev)
{
	struct artc_priv *priv = netdev_priv(dev);
	struct can_bittiming *bt = &priv->can.bittiming;
	u32 btr0, btr1;

	btr0 = ((bt->brp - 1) & 0x3f) | (((bt->sjw - 1) & 0x3) << 6);
	btr1 = ((bt->prop_seg + bt->phase_seg1 - 1) & 0xf) |
		(((bt->phase_seg2 - 1) & 0x7) << 4);

	if (priv->can.ctrlmode & CAN_CTRLMODE_3_SAMPLES)
		btr1 |= 0x80;
	netdev_dbg(dev, "setting BTR0=0x%02x BTR1=0x%02x\n", btr0, btr1);

	writel(btr0, priv->base + ARTC_CAN_BTR0_REG);
	writel(btr1, priv->base + ARTC_CAN_BTR1_REG);
	return 0;
}

static int artc_can_get_berr_counter(const struct net_device *dev,
				     struct can_berr_counter *bec)
{
	struct artc_priv *priv = netdev_priv(dev);
	u32 errors;

	errors = clk_prepare_enable(priv->clk);
	if (errors) {
		dev_err(priv->dev, "could not enable clock\n");
		return errors;
	}
	errors = readl(priv->base + ARTC_CAN_RXERR_REG);
	bec->rxerr = errors & 0xff;

	errors = readl(priv->base + ARTC_CAN_TXERR_REG);
	bec->txerr = errors & 0xff;

	clk_disable_unprepare(priv->clk);

	return 0;
}

static int artc_can_start(struct net_device *dev)
{
	struct artc_priv *priv = netdev_priv(dev);
	int err;
	u32 mod_reg_val;
	int i;

	/* Set reset mode */
	err = set_reset_mode(dev);
	if (err) {
		netdev_err(dev, "could not enter reset mode\n");
		return err;
	}

	/* Set filters, accept all data */
	for (i = 0; i < 4; i++)
		writel(0xff, priv->base + ARTC_CAN_RXMASK0_REG + (i * 4));

	/* clear tx and rx error counters */
	writel(0, priv->base + ARTC_CAN_RXERR_REG);
	writel(0, priv->base + ARTC_CAN_TXERR_REG);

	/* enable interrupts */
	if (priv->can.ctrlmode & CAN_CTRLMODE_BERR_REPORTING)
		writel(0xff, priv->base + ARTC_CAN_INTEN_REG);
	else
		writel(0xff & (~ARTC_CAN_INTEN_ERRB),
		       priv->base + ARTC_CAN_INTEN_REG);

	/* enter the selected mode */
	mod_reg_val = readl(priv->base + ARTC_CAN_MODE_REG);
	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
		mod_reg_val |= ARTC_CAN_MODE_SELFTEST;
	else if (priv->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)
		mod_reg_val |= ARTC_CAN_MODE_LISTEN;
	writel(mod_reg_val, priv->base + ARTC_CAN_MODE_REG);

	err = artc_can_set_bittiming(dev);
	if (err)
		return err;

	/* enter into normal mode */
	err = set_normal_mode(dev);
	if (err) {
		netdev_err(dev, "could not enter normal mode\n");
		return err;
	}

	priv->can.state = CAN_STATE_ERROR_ACTIVE;

	return 0;
}

static int artc_can_stop(struct net_device *dev)
{
	struct artc_priv *priv = netdev_priv(dev);
	int err;

	priv->can.state = CAN_STATE_STOPPED;
	/* enter reset mode */
	err = set_reset_mode(dev);
	if (err) {
		netdev_err(dev, "could not enter reset mode\n");
		return err;
	}

	/* disable all interrupts */
	writel(0, priv->base + ARTC_CAN_INTEN_REG);

	return 0;
}

static int artc_can_set_mode(struct net_device *dev, enum can_mode mode)
{
	int err;

	switch (mode) {
	case CAN_MODE_START:
		err = artc_can_start(dev);
		if (err) {
			netdev_err(dev, "starting CAN controller failed\n");
			return err;
		}

		if (netif_queue_stopped(dev))
			netif_wake_queue(dev);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

/* transmit a CAN message.
 * The message layout in the sk_buff should be like this:
 * xx xx xx xx	 ff	 ll   00 11 22 33 44 55 66 77
 * [  can-id ] [flags] [len] [can data (up to 8 bytes]
 */
static netdev_tx_t artc_can_start_xmit(struct sk_buff *skb,
				       struct net_device *dev)
{
	struct artc_priv *priv = netdev_priv(dev);
	struct can_frame *cf = (struct can_frame *)skb->data;
	u8 dlc;
	u32 dreg, msg_flag_n;
	canid_t id;
	int i;

	if (can_dropped_invalid_skb(dev, skb))
		return NETDEV_TX_OK;

	netif_stop_queue(dev);

	id = cf->can_id;
	dlc = cf->can_dlc;
	msg_flag_n = dlc;

	if (id & CAN_RTR_FLAG)
		msg_flag_n |= ARTC_CAN_BUF0_MSG_RTR_FLAG;

	if (id & CAN_EFF_FLAG) {
		msg_flag_n |= ARTC_CAN_BUF0_MSG_EFF_FLAG;
		dreg = ARTC_CAN_BUF5_REG;
		writel((id >> 21) & 0xff, priv->base + ARTC_CAN_BUF1_REG);
		writel((id >> 13) & 0xff, priv->base + ARTC_CAN_BUF2_REG);
		writel((id >> 5) & 0xff, priv->base + ARTC_CAN_BUF3_REG);
		writel((id & 0x1f) << 3, priv->base + ARTC_CAN_BUF4_REG);
	} else {
		dreg = ARTC_CAN_BUF3_REG;
		writel((id >> 3) & 0xff, priv->base + ARTC_CAN_BUF1_REG);
		writel((id & 0x7) << 5, priv->base + ARTC_CAN_BUF2_REG);
	}

	for (i = 0; i < dlc; i++)
		writel(cf->data[i], priv->base + dreg + i * 4);

	writel(msg_flag_n, priv->base + ARTC_CAN_BUF0_REG);

	can_put_echo_skb(skb, dev, 0);

	if (priv->can.ctrlmode & CAN_CTRLMODE_LOOPBACK)
		writel(ARTC_CAN_MCR_SELFREQ, priv->base + ARTC_CAN_MCR_REG);
	else
		writel(ARTC_CAN_MCR_TXREQ, priv->base + ARTC_CAN_MCR_REG);

	return NETDEV_TX_OK;
}

static void artc_can_rx(struct net_device *dev)
{
	struct artc_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	u8 fi;
	u32 dreg;
	canid_t id;
	int i;

	/* create zero'ed CAN frame buffer */
	skb = alloc_can_skb(dev, &cf);
	if (!skb)
		return;

	fi = readl(priv->base + ARTC_CAN_BUF0_REG);
	cf->can_dlc = get_can_dlc(fi & 0x0f);
	if (fi & ARTC_CAN_BUF0_MSG_EFF_FLAG) {
		dreg = ARTC_CAN_BUF5_REG;
		id = (readl(priv->base + ARTC_CAN_BUF1_REG) << 21) |
		     (readl(priv->base + ARTC_CAN_BUF2_REG) << 13) |
		     (readl(priv->base + ARTC_CAN_BUF3_REG) << 5) |
		     ((readl(priv->base + ARTC_CAN_BUF4_REG) >> 3) & 0x1f);
		id |= CAN_EFF_FLAG;
	} else {
		dreg = ARTC_CAN_BUF3_REG;
		id = (readl(priv->base + ARTC_CAN_BUF1_REG) << 3) |
		     ((readl(priv->base + ARTC_CAN_BUF2_REG) >> 5) & 0x7);
	}

	/* check if it is a remote frame, remote frame has no data */
	if (fi & ARTC_CAN_BUF0_MSG_RTR_FLAG)
		id |= CAN_RTR_FLAG;
	else
		for (i = 0; i < cf->can_dlc; i++)
			cf->data[i] = readl(priv->base + dreg + i * 4);

	cf->can_id = id;
	/* Release CAN frame in RX FIFO */
	writel(ARTC_CAN_MCR_RXB_REL, priv->base + ARTC_CAN_MCR_REG);

	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;
	netif_rx(skb);

	can_led_event(dev, CAN_LED_EVENT_RX);
}

static int artc_can_err(struct net_device *dev, u8 isrc, u8 status)
{
	struct artc_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	enum can_state state = priv->can.state;
	enum can_state rx_state, tx_state;
	unsigned int rxerr, txerr;
	u32 ecc, alc;

	skb = alloc_can_err_skb(dev, &cf);
	if (!skb)
		return -ENOMEM;

	rxerr = readl(priv->base + ARTC_CAN_RXERR_REG);
	txerr = readl(priv->base + ARTC_CAN_TXERR_REG);

	cf->data[6] = txerr;
	cf->data[7] = rxerr;
	if (isrc & ARTC_CAN_INTR_OVF) {
		/* data overflow interrupt */
		netdev_dbg(dev, "data overrun interrupt\n");
		if (likely(skb)) {
			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] = CAN_ERR_CRTL_RX_OVERFLOW;
		}
		stats->rx_over_errors++;
		stats->rx_errors++;

		/* reset the CAN IP by entering reset mode */
		set_reset_mode(dev);
		set_normal_mode(dev);
		/* clear bit */
		writel(ARTC_CAN_MCR_CLR_OVF, priv->base + ARTC_CAN_MCR_REG);
	}

	if (isrc & ARTC_CAN_INTR_ERRW) {
		/* error warning interrupt */
		netdev_dbg(dev, "error warning interrupt\n");
		if (status & ARTC_CAN_STAT_BUS)
			state = CAN_STATE_BUS_OFF;
		else if (status & ARTC_CAN_STAT_ERR)
			state = CAN_STATE_ERROR_WARNING;
		else
			state = CAN_STATE_ERROR_ACTIVE;
	}

	if (isrc & ARTC_CAN_INTR_ERRB) {
		/* bus error interrupt */
		netdev_dbg(dev, "bus error interrupt\n");
		priv->can.can_stats.bus_error++;
		stats->rx_errors++;

		ecc = readl(priv->base + ARTC_CAN_ERRCODE_REG);
		cf->can_id |= CAN_ERR_PROT | CAN_ERR_BUSERROR;
		switch (ecc & ARTC_CAN_ERRCODE_ERRTYPE_MASK) {
		case ARTC_CAN_ERRCODE_ERRTYPE_BIT:
			cf->data[2] |= CAN_ERR_PROT_BIT;
			break;
		case ARTC_CAN_ERRCODE_ERRTYPE_FORMAT:
			cf->data[2] |= CAN_ERR_PROT_FORM;
			break;
		case ARTC_CAN_ERRCODE_ERRTYPE_STUFF:
			cf->data[2] |= CAN_ERR_PROT_STUFF;
			break;
		default:
			break;
		}

		/* Set error location */
		cf->data[3] = ecc & ARTC_CAN_ERRCODE_SEGCODE_MASK;

		/* Error occurred during transmission */
		if ((ecc & ARTC_CAN_ERRCODE_DIR) == 0)
			cf->data[2] |= CAN_ERR_PROT_TX;
	}

	if (isrc & ARTC_CAN_INTR_ERRP) {
		/* error passive interrupt */
		netdev_dbg(dev, "error passive interrupt\n");
		if (state == CAN_STATE_ERROR_PASSIVE)
			state = CAN_STATE_ERROR_WARNING;
		else
			state = CAN_STATE_ERROR_PASSIVE;
	}

	if (isrc & ARTC_CAN_INTR_ARBLOST) {
		/* arbitration lost interrupt */
		netdev_dbg(dev, "arbitration lost interrupt\n");
		alc = readl(priv->base + ARTC_CAN_ARBLOST_REG);
		priv->can.can_stats.arbitration_lost++;
		stats->tx_errors++;
		cf->can_id |= CAN_ERR_LOSTARB;
		cf->data[0] = alc & 0x1f;
	}

	if (state != priv->can.state) {
		tx_state = txerr >= rxerr ? state : 0;
		rx_state = txerr <= rxerr ? state : 0;

		can_change_state(dev, cf, tx_state, rx_state);

		if (state == CAN_STATE_BUS_OFF)
			can_bus_off(dev);
	}

	stats->rx_packets++;
	stats->rx_bytes += cf->can_dlc;
	netif_rx(skb);

	return 0;
}

static irqreturn_t artc_can_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct artc_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	u8 isrc, status;
	int n = 0;

	while ((isrc = readl(priv->base + ARTC_CAN_INTR_REG)) &&
	       (n < ARTC_CAN_MAX_IRQ)) {
		status = readl(priv->base + ARTC_CAN_STAT_REG);

		if (isrc & ARTC_CAN_INTR_WAKEUP)
			netdev_warn(dev, "wakeup interrupt\n");

		if (isrc & ARTC_CAN_INTR_TX) {
			/* transmission complete interrupt */
			stats->tx_bytes += readl(priv->base +
						 ARTC_CAN_TXBRO_REG) & 0xf;
			stats->tx_packets++;
			can_get_echo_skb(dev, 0);
			netif_wake_queue(dev);
			can_led_event(dev, CAN_LED_EVENT_TX);
		}

		if ((isrc & ARTC_CAN_INTR_RX) &&
		     !(isrc & ARTC_CAN_INTR_OVF)) {
			/* receive interrupt, don't read if overrun occurred */
			while (status & ARTC_CAN_STAT_RXB) {
				/* RX buffer is not empty */
				artc_can_rx(dev);
				status = readl(priv->base + ARTC_CAN_STAT_REG);
			}
		}

		if (isrc & (ARTC_CAN_INTR_ARBLOST | ARTC_CAN_INTR_ERRB |
			    ARTC_CAN_INTR_ERRP | ARTC_CAN_INTR_ERRW |
			    ARTC_CAN_INTR_OVF)) {
			/* error interrupt */
			if (artc_can_err(dev, isrc, status))
				netdev_err(dev, "can't allocate buffer\n");
		}

		/* clear interrupts */
		writel(isrc, priv->base + ARTC_CAN_INTR_REG);
		n++;
	}

	if (n >= ARTC_CAN_MAX_IRQ)
		netdev_dbg(dev, "%d message handled in ISR\n", n);

	return (n) ? IRQ_HANDLED : IRQ_NONE;
}

static int artc_can_open(struct net_device *dev)
{
	struct artc_priv *priv = netdev_priv(dev);
	int err;

	/* common open */
	err = open_candev(dev);
	if (err)
		return err;

	/* register interrupt handler */
	err = request_irq(dev->irq, artc_can_interrupt, 0, dev->name, dev);
	if (err) {
		netdev_err(dev, "request_irq err: %d\n", err);
		goto exit_irq;
	}

	/* turn on clocking for CAN module */
	err = clk_prepare_enable(priv->clk);
	if (err) {
		netdev_err(dev, "could not enable CAN clock\n");
		goto exit_clock;
	}

	err = artc_can_start(dev);
	if (err) {
		netdev_err(dev, "could not start CAN peripheral\n");
		goto exit_can_start;
	}

	can_led_event(dev, CAN_LED_EVENT_OPEN);
	netif_start_queue(dev);

	return 0;

exit_can_start:
	clk_disable_unprepare(priv->clk);
exit_clock:
	free_irq(dev->irq, dev);
exit_irq:
	close_candev(dev);

	return err;
}

static int artc_can_close(struct net_device *dev)
{
	struct artc_priv *priv = netdev_priv(dev);

	netif_stop_queue(dev);
	artc_can_stop(dev);
	clk_disable_unprepare(priv->clk);

	free_irq(dev->irq, dev);
	close_candev(dev);
	can_led_event(dev, CAN_LED_EVENT_STOP);

	return 0;
}

static const struct net_device_ops artc_can_netdev_ops = {
	.ndo_open = artc_can_open,
	.ndo_stop = artc_can_close,
	.ndo_start_xmit = artc_can_start_xmit,
};

static int artc_can_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *res;
	struct net_device *ndev;
	struct artc_priv *priv;
	struct clk *clk;
	struct reset_control *rst;
	void __iomem *addr;
	int err, irq;

	clk = of_clk_get(np, 0);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "unable to request clock\n");
		err = -ENODEV;
		goto exit;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		err = -ENODEV;
		goto exit;
	}

	rst = of_reset_control_get_exclusive(np, NULL);
	if (IS_ERR(rst)) {
		dev_err(&pdev->dev, "failed to get CAN reset control\n");
		return PTR_ERR(rst);
	}

	err = reset_control_deassert(rst);
	if (err) {
		dev_err(&pdev->dev, "failed to deassert CAN reset control\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr)) {
		err = -EBUSY;
		goto exit;
	}

	ndev = alloc_candev(sizeof(struct artc_priv), 1);
	if (!ndev) {
		dev_err(&pdev->dev,
			"could not allocate memory for CAN driver\n");
		err = -ENOMEM;
		goto exit;
	}

	ndev->netdev_ops = &artc_can_netdev_ops;
	ndev->irq = irq;
	ndev->flags |= IFF_ECHO;

	priv = netdev_priv(ndev);
	priv->dev = &pdev->dev;
	priv->can.clock.freq = clk_get_rate(clk);
	priv->can.bittiming_const = &artccan_bittiming_const;
	priv->can.do_set_mode = artc_can_set_mode;
	priv->can.do_get_berr_counter = artc_can_get_berr_counter;
	priv->can.ctrlmode_supported = CAN_CTRLMODE_LOOPBACK |
				       CAN_CTRLMODE_LISTENONLY |
				       CAN_CTRLMODE_3_SAMPLES |
				       CAN_CTRLMODE_ONE_SHOT |
				       CAN_CTRLMODE_BERR_REPORTING |
				       CAN_CTRLMODE_PRESUME_ACK;
	priv->base = addr;
	priv->clk = clk;
	priv->rst = rst;
	spin_lock_init(&priv->cmdreg_lock);

	platform_set_drvdata(pdev, ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);

	err = register_candev(ndev);
	if (err) {
		dev_err(&pdev->dev,
			"register %s failed (err=%d)\n", DRV_NAME, err);
		goto exit_free;
	}

	devm_can_led_init(ndev);

	dev_info(&pdev->dev, "device registered (base=%p, irq=%d)\n",
		 priv->base, ndev->irq);

	return 0;

exit_free:
	free_candev(ndev);
exit:
	return err;
}

static int artc_can_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);

	unregister_netdev(ndev);
	free_candev(ndev);

	return 0;
}

#ifdef CONFIG_PM
static int __maybe_unused artc_can_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct artc_priv *priv = netdev_priv(ndev);

	clk_disable_unprepare(priv->clk);
	return 0;
}

static int __maybe_unused artc_can_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct artc_priv *priv = netdev_priv(ndev);
	int ret;

	ret = clk_prepare_enable(priv->clk);
	if (ret) {
		dev_err(dev, "Cannot enable clock\n");
		return ret;
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(artc_can_pm_ops, artc_can_suspend,
					artc_can_resume);

#define ARTC_CAN_DEV_PM_OPS			(&artc_can_pm_ops)
#else
#define ARTC_CAN_DEV_PM_OPS			NULL
#endif

static const struct of_device_id artc_can_of_match[] = {
	{.compatible = "artc,artc-can"},
	{},
};

MODULE_DEVICE_TABLE(of, artc_can_of_match);

static struct platform_driver artc_can_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = artc_can_of_match,
		.pm = ARTC_CAN_DEV_PM_OPS,
	},
	.probe = artc_can_probe,
	.remove = artc_can_remove,
};

module_platform_driver(artc_can_driver);

MODULE_DESCRIPTION("artc CAN controller driver");
MODULE_AUTHOR("zhangsan <zhangsan@artc.com>");
MODULE_LICENSE("GPL");

