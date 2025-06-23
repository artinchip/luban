// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for ArtInChip CIR controller
 *
 * Copyright (C) 2021 Artinchip Technology Co., Ltd.
 * Authors: Weijie Ding <weijie.ding@artinchip.com>
 */
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <media/rc-core.h>
#include <linux/delay.h>
#include <linux/bits.h>

#define AIC_IR_DEV	"aic-ir"
#define DEFAULT_FREQ	38000
#define US_PER_SEC	1000000UL

/* Register definition */
#define CIR_MCR_REG			0x00
#define CIR_MCR_TXFIFO_CLR		BIT(17)
#define CIR_MCR_RXFIFO_CLR		BIT(16)
#define CIR_MCR_TX_STOP			BIT(9)
#define CIR_MCR_TX_START		BIT(8)
#define CIR_MCR_TX_EN			BIT(1)
#define CIR_MCR_RX_EN			BIT(0)

#define CIR_INTR_REG			0x04
#define CIR_INTR_TXB_AVL_INT		BIT(6)
#define CIR_INTR_TXEND_INT		BIT(5)
#define CIR_INTR_TX_UNF_INT		BIT(4)
#define CIR_INTR_RXB_AVL_INT		BIT(2)
#define CIR_INTR_RX_END_INT		BIT(1)
#define CIR_INTR_RX_OVF_INT		BIT(0)

#define CIR_INTEN_REG			0x08
#define CIR_INTEN_TXB_EMPTY_LEVEL_POS	(16)
#define CIR_INTEN_TXB_EMPTY_LEVEL(x)	((x) << CIR_INTEN_TXB_EMPTY_LEVEL_POS)
#define CIR_INTEN_RXB_AVL_LEVEL_POS	(8)
#define CIR_INTEN_RXB_AVL_LEVEL(x)	((x) << CIR_INTEN_RXB_AVL_LEVEL_POS)
#define CIR_INTEN_TXB_AVL_EN		BIT(6)
#define CIR_INTEN_TXEND_EN		BIT(5)
#define CIR_INTEN_TX_UNF_EN		BIT(4)
#define CIR_INTEN_RXB_AVL_EN		BIT(2)
#define CIR_INTEN_RXEND_EN		BIT(1)
#define CIR_INTEN_RX_OVF_EN		BIT(0)

#define CIR_TXSTAT_REG			0x0C
#define CIR_TXSTAT_TX_STA		(16)
#define CIR_TXSTAT_TXFIFO_ERR		(10)
#define CIR_TXSTAT_TXFIFO_FULL		(9)
#define CIR_TXSTAT_TXFIFO_EMPTY 	BIT(8)
#define CIR_TXSTAT_TXFIFO_DLEN_MASK	GENMASK(6, 0)

#define CIR_RXSTAT_REG			0x10
#define CIR_RXSTAT_RX_STA		(16)
#define CIR_RXSTAT_RXFIFO_ERR		(10)
#define CIR_RXSTAT_RXFIFO_FULL		(9)
#define CIR_RXSTAT_RXFIFO_EMPTY		BIT(8)
#define CIR_RXSTAT_RXFIFO_DLEN		(0)

#define CIR_RXCLK_REG			0x14
#define CIR_RX_THRES_REG		0x18
#define CIR_RX_THRES_ACTIVE		(16)
#define CIR_RX_THRES_ACTIVE_LEVEL(x)	((x) << CIR_RX_THRES_ACTIVE)
#define CIR_RX_THRES_IDLE		(0)
#define CIR_RX_THRES_IDLE_LEVEL(x)	((x) << CIR_RX_THRES_IDLE)

#define CIR_RX_CFG_REG			0x1C
#define CIR_RX_CFG_NOISE_V1		(16)
#define CIR_RX_CFG_NOISE_MASK_V1	0xFFFF
#define RX_NOISE_LEVEL_V1		0x1770
#define CIR_RX_CFG_NOISE		(8)
#define CIR_RX_CFG_NOISE_MASK		0xFF
#define RX_NOISE_LEVEL			0xFF
#define CIR_RX_CFG_NOISE_LEVEL(x)	\
	((x & CIR_RX_CFG_NOISE_MASK) << CIR_RX_CFG_NOISE)
#define CIR_RX_CFG_NOISE_LEVEL_V1(x)	\
	((x & CIR_RX_CFG_NOISE_MASK_V1) << CIR_RX_CFG_NOISE_V1)
#define CIR_RX_CFG_RX_LEVEL		BIT(1)
#define CIR_RX_CFG_RX_INVERT		BIT(0)

#define CIR_TX_CFG_REG			0x20
#define CIR_TX_CFG_TX_MODE		BIT(2)
#define CIR_TX_CFG_TX_OUT_MODE		BIT(1)
#define CIR_TX_CFG_TX_INVERT		BIT(0)

#define CIR_TIDC_REG			0x24
#define CIR_CARR_CFG_REG		0x2C
#define CIR_CARR_CFG_HIGH		(16)
#define CIR_CARR_CFG_HIGH_VAL(x)	((x) << CIR_CARR_CFG_HIGH)
#define CIR_CARR_CFG_LOW		(0)
#define CIR_CARR_CFG_LOW_VAL(x)		((x) << CIR_CARR_CFG_LOW)

#define CIR_RXFIFO_REG			0x30
#define CIR_TXFIFO_REG			0x80
#define CIR_VERSION_REG			0xFFC

#define CIR_TXFIFO_SIZE 		0x80

struct aic_ir {
	spinlock_t	ir_lock;
	struct rc_dev	*rc;
	void __iomem	*base;
	struct clk	*clk;
	struct reset_control *rst;
	const char	*map_name;
	unsigned int	tx_duty;
	int		irq;
	u32		rx_level;  /* Indicates the idle level of RX */
	u8		rx_flag;   /* Indicates if rxfifo has received data */
	unsigned int 	*tx_buf;
	u32		tx_size;
	u32		tx_idx;
	struct completion tx_complete;

};

void aic_tx_fifo_clear(struct aic_ir *ir)
{
	unsigned int val;

	val = readl(ir->base + CIR_MCR_REG);
	val |= CIR_MCR_TXFIFO_CLR;
	writel(val, ir->base + CIR_MCR_REG);
}

void aic_tx_int_enable(struct aic_ir *ir)
{
	unsigned int val;

	val = readl(ir->base + CIR_INTEN_REG);
	val |= CIR_INTEN_TXB_AVL_EN | CIR_INTEN_TXEND_EN | CIR_INTEN_TX_UNF_EN;
	writel(val, ir->base + CIR_INTEN_REG);
}

void aic_tx_int_disable(struct aic_ir *ir)
{
	unsigned int val;

	val = readl(ir->base + CIR_INTEN_REG);
	val &= ~(CIR_INTEN_TXB_AVL_EN |
		 CIR_INTEN_TXEND_EN | CIR_INTEN_TX_UNF_EN);
	writel(val, ir->base + CIR_INTEN_REG);
}

void aic_cir_tx_handle(struct aic_ir *ir)
{
	unsigned int tx_status, tx_dlen, free_space, i;
	unsigned int rlc_max, val, duration;

	tx_status = readl(ir->base + CIR_TXSTAT_REG);
	tx_dlen = tx_status & CIR_TXSTAT_TXFIFO_DLEN_MASK;

	if (tx_status & CIR_TXSTAT_TXFIFO_EMPTY)
		free_space = CIR_TXFIFO_SIZE;
	else
		free_space = CIR_TXFIFO_SIZE - tx_dlen - 1;

	rlc_max = 128 * ir->rc->tx_resolution;
	/*
	 * For each level toggle, generate RLC code according to
	 * tx_buf[i] value
	 */
	for (i = ir->tx_idx; (i < ir->tx_size) && free_space; i++) {
		duration = ir->tx_buf[i];

		if (DIV_ROUND_UP(duration, rlc_max) > free_space)
			break;

		while (duration > rlc_max) {
			val = (((i & 1) == 0) << 7) | 0x7F;
			writel(val, ir->base + CIR_TXFIFO_REG);
			free_space--;
			duration -= rlc_max;
		}

		duration = duration / ir->rc->tx_resolution;
		val = (((i & 1) == 0) << 7) | duration;
		writel(val, ir->base + CIR_TXFIFO_REG);
		free_space--;
	}

	ir->tx_idx = i;
}

static irqreturn_t aic_ir_irq(int irqno, void *dev_id)
{
	unsigned int int_status;
	unsigned int rx_status;
	unsigned int data;
	unsigned int i, count;
	struct aic_ir *ir = dev_id;
	struct ir_raw_event rawir = {};

	spin_lock(&ir->ir_lock);
	int_status = readl(ir->base + CIR_INTR_REG);
	rx_status = readl(ir->base + CIR_RXSTAT_REG);

	/* clear all pending status */
	writel(0xFF, ir->base + CIR_INTR_REG);
	/* Handle Receive data */
	if (int_status & (CIR_INTR_RXB_AVL_INT | CIR_INTR_RX_END_INT)) {
		/* Get the number of data in RXFIFO */
		count = rx_status & 0x3F;
		/* Confirm if RXFIFO has data */
		if (!(rx_status & (0x1 << 8))) {
			for (i = 0; i < count; i++) {
				data = readl(ir->base + CIR_RXFIFO_REG);
				rawir.pulse = ((data & 0x80) != 0) ^
						ir->rx_level;
				rawir.duration = ((data & 0x7F) + 1) *
						 ir->rc->rx_resolution;
				ir_raw_event_store_with_filter(ir->rc, &rawir);
			}
			ir->rx_flag = 1;
		}
	}

	if (int_status & CIR_INTR_RX_OVF_INT) {
		ir_raw_event_reset(ir->rc);
		ir->rx_flag = 0;
	} else if ((int_status & CIR_INTR_RX_END_INT) && ir->rx_flag) {
		ir_raw_event_set_idle(ir->rc, true);
		ir_raw_event_handle(ir->rc);
		ir->rx_flag = 0;
	}

	if (int_status & CIR_INTR_TXB_AVL_INT)
		aic_cir_tx_handle(ir);

	if (int_status & CIR_INTR_TXEND_INT) {
		aic_tx_int_disable(ir);
		complete(&ir->tx_complete);
	}

	spin_unlock(&ir->ir_lock);
	return IRQ_HANDLED;
}

static int aic_set_rx_carrier_range(struct rc_dev *rcdev, u32 min, u32 max)
{
	unsigned int clk_div = 0;
	unsigned int mod_clk = 0;
	struct aic_ir *ir = rcdev->priv;

	/* Get CIR module clock */
	mod_clk = clk_get_rate(ir->clk);
	/* Calculate clock divider */
	clk_div = DIV_ROUND_UP(mod_clk, max);
	dev_dbg(&rcdev->dev, "max: %d, clk_div: %d\n", max, clk_div);
	writel(clk_div - 1, ir->base + CIR_RXCLK_REG);
	/* Re-calculate rx-resolution and tx-resolution */
	rcdev->rx_resolution = (US_PER_SEC / max);
	dev_dbg(&rcdev->dev, "rx_resolution: %d\n", rcdev->rx_resolution);
	return 0;
}

static int aic_set_tx_duty_cycle(struct rc_dev *rcdev, u32 duty_cycle)
{
	struct aic_ir *ir = rcdev->priv;

	if (duty_cycle < 1 || duty_cycle > 99)
		return -EINVAL;

	ir->tx_duty = duty_cycle;
	return 0;
}

static int aic_set_tx_carrier(struct rc_dev *rcdev, u32 carrier)
{
	unsigned int val = 0;
	unsigned int clk_div = 0;
	unsigned int carrier_high;
	unsigned int carrier_low;
	unsigned int mod_clk = 0;
	struct aic_ir *ir = rcdev->priv;

	/* Get CIR module clock */
	mod_clk = clk_get_rate(ir->clk);
	/* Calculate clock divider */
	clk_div = DIV_ROUND_UP(mod_clk, carrier);
	carrier_high = mod_clk / carrier * ir->tx_duty / 100;
	carrier_low = clk_div - carrier_high;

	val = (carrier_high << 16) | carrier_low;
	writel(val, ir->base + CIR_CARR_CFG_REG);
	rcdev->tx_resolution = (US_PER_SEC / carrier);
	return 0;
}

static int aic_tx_ir(struct rc_dev *rcdev, unsigned int *txbuf,
		    unsigned int count)
{
	ssize_t ret = 0;
	unsigned long timeout;
	unsigned int val;
	struct aic_ir *ir = rcdev->priv;

	ir->tx_size = count;
	ir->tx_idx  = 0;
	ir->tx_buf  = txbuf;
	aic_tx_fifo_clear(ir);
	aic_cir_tx_handle(ir);

	/* TX Interrupt Enable */
	aic_tx_int_enable(ir);

	/* Start to transfer */
	val = readl(ir->base + CIR_MCR_REG);
	val |= CIR_MCR_TX_START;
	writel(val, ir->base + CIR_MCR_REG);

	timeout = wait_for_completion_timeout(&ir->tx_complete,
					      msecs_to_jiffies(2000));
	if (!timeout)
		ret = -EIO;

	return ret;
}

static int aic_cir_probe(struct platform_device *pdev)
{
	struct aic_ir *ir;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *dn = dev->of_node;
	unsigned int val = 0;
	unsigned int mod_clk = 0;
	unsigned int clk_div = 0;
	unsigned int carrier_high = 0;
	unsigned int carrier_low = 0;
	int ret = 0;

	ir = devm_kzalloc(dev, sizeof(struct aic_ir), GFP_KERNEL);
	if (!ir)
		return -ENOMEM;

	spin_lock_init(&ir->ir_lock);

	ir->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ir->clk)) {
		dev_err(dev, "Failed to get CIR clock\n");
		return PTR_ERR(ir->clk);
	}

	if (clk_prepare_enable(ir->clk)) {
		dev_err(dev, "try to enable CIR clock failed\n");
		return -EINVAL;
	}

	ir->rst = devm_reset_control_get_exclusive(dev, NULL);
	if (IS_ERR(ir->rst))
		return PTR_ERR(ir->rst);
	ret = reset_control_deassert(ir->rst);
	if (ret)
		goto exit_clk_disable;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ir->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(ir->base)) {
		ret = PTR_ERR(ir->base);
		goto exit_reset_assert;
	}

	ir->rc = rc_allocate_device(RC_DRIVER_IR_RAW);
	if (!ir->rc) {
		dev_err(dev, "Failed to allocate device\n");
		ret = -ENOMEM;
		goto exit_reset_assert;
	}

	ir->tx_duty = 33;	/* Carrier duty: 33% */
	ir->rc->priv = ir;
	ir->rc->device_name = AIC_IR_DEV;
	ir->rc->input_phys = "aic-ir/input0";
	ir->rc->input_id.bustype = BUS_HOST;
	ir->rc->input_id.vendor = 0x0001;
	ir->rc->input_id.product = 0x0001;
	ir->rc->input_id.version = 0x0100;
	ir->map_name = of_get_property(dn, "linux,rc-map-name", NULL);
	ir->rc->map_name = ir->map_name ? : RC_MAP_EMPTY;
	ir->rc->dev.parent = dev;
	ir->rc->allowed_protocols = RC_PROTO_BIT_ALL_IR_DECODER;
	/* Frequency after IR internal divider with sample period in ns */
	ir->rc->rx_resolution = (US_PER_SEC / DEFAULT_FREQ);
	ir->rc->tx_resolution = (US_PER_SEC / DEFAULT_FREQ);
	ir->rc->min_timeout = 1;
	ir->rc->max_timeout = 10 * IR_DEFAULT_TIMEOUT;
	ir->rc->driver_name = AIC_IR_DEV;
	ir->rc->s_rx_carrier_range = aic_set_rx_carrier_range;
	ir->rc->s_tx_carrier = aic_set_tx_carrier;
	ir->rc->s_tx_duty_cycle = aic_set_tx_duty_cycle;
	ir->rc->tx_ir = aic_tx_ir;
	ret = rc_register_device(ir->rc);
	if (ret) {
		dev_err(dev, "Failed to register rc device\n");
		goto exit_free_dev;
	}

	platform_set_drvdata(pdev, ir);

	if (of_property_read_u32(dn, "rx-level", &ir->rx_level)) {
		dev_err(dev, "failed to get rx-level property\n");
		return -EINVAL;
	}

	if (of_property_read_bool(dn, "tx-invert")) {
		val = readl(ir->base + CIR_TX_CFG_REG);
		val |= CIR_TX_CFG_TX_INVERT;
		writel(val, ir->base + CIR_TX_CFG_REG);
	}

	/* Set RX CFG */
	val = ir->rx_level << 1;
	if (of_property_read_bool(dn, "rx-invert"))
		val |= CIR_RX_CFG_RX_INVERT;

	if (of_device_is_compatible(dn, "artinchip,aic-cir-v1.0"))
		val |= CIR_RX_CFG_NOISE_LEVEL_V1(RX_NOISE_LEVEL_V1);
	else
		val |= CIR_RX_CFG_NOISE_LEVEL(RX_NOISE_LEVEL);
	writel(val, ir->base + CIR_RX_CFG_REG);
	/* Set RX active and idle threshold */
	writel((CIR_RX_THRES_ACTIVE_LEVEL(0x10) |
		CIR_RX_THRES_IDLE_LEVEL(0x400)),
		ir->base + CIR_RX_THRES_REG);
	/* Set RX default sample clock, Default protocol is NEC */
	mod_clk = clk_get_rate(ir->clk);
	clk_div = DIV_ROUND_UP(mod_clk, DEFAULT_FREQ);
	writel(clk_div - 1, ir->base + CIR_RXCLK_REG);

	/* Configure carrier, Default protocol is NEC, 38000 */
	carrier_high = mod_clk / DEFAULT_FREQ * ir->tx_duty /100;
	carrier_low = clk_div - carrier_high;
	writel((carrier_high << 16) | carrier_low,
	      ir->base + CIR_CARR_CFG_REG);

	/* Flush TX and RX FIFO */
	writel((CIR_MCR_RXFIFO_CLR | CIR_MCR_TXFIFO_CLR),
	      ir->base + CIR_MCR_REG);
	/* Set TX and RX FIFO level */
	writel((CIR_INTEN_TXB_EMPTY_LEVEL(0x40) |
		CIR_INTEN_RXB_AVL_LEVEL(0x20)),
		ir->base + CIR_INTEN_REG);
	/* Clear pending interrupt flags */
	writel(0xff, ir->base + CIR_INTR_REG);
	/* Interrupt Enable */
	val = readl(ir->base + CIR_INTEN_REG) & (~0xFF);
	val |= 0x7;
	writel(val, ir->base + CIR_INTEN_REG);

	/* IRQ */
	ir->irq = platform_get_irq(pdev, 0);
	if (ir->irq < 0) {
		ret = ir->irq;
		goto exit_free_dev;
	}

	ret = devm_request_irq(dev, ir->irq, aic_ir_irq, 0, AIC_IR_DEV, ir);
	if (ret) {
		dev_err(dev, "Failed to request irq\n");
		goto exit_free_dev;
	}

	init_completion(&ir->tx_complete);

	/* Enable Receiver and Transmitter */
	val = readl(ir->base + CIR_MCR_REG);
	val |= (0x3);
	writel(val, ir->base + CIR_MCR_REG);

	dev_info(dev, "Initialized AIC IR driver\n");
	return 0;

exit_free_dev:
	rc_free_device(ir->rc);
exit_reset_assert:
	reset_control_assert(ir->rst);
exit_clk_disable:
	clk_disable_unprepare(ir->clk);

	return ret;
}

static int aic_cir_remove(struct platform_device *pdev)
{
	unsigned long flags;
	struct aic_ir *ir = platform_get_drvdata(pdev);

	clk_disable_unprepare(ir->clk);
	reset_control_assert(ir->rst);

	spin_lock_irqsave(&ir->ir_lock, flags);
	/* Disable IR */
	writel(0, ir->base + CIR_MCR_REG);
	spin_unlock_irqrestore(&ir->ir_lock, flags);

	rc_unregister_device(ir->rc);
	return 0;
}

static const struct of_device_id aic_cir_match[] = {
	{
		.compatible = "artinchip,aic-cir-v0.1",
	},
	{
		.compatible = "artinchip,aic-cir-v1.0",
	},
	{}
};

MODULE_DEVICE_TABLE(of, aic_cir_match);

static struct platform_driver aic_cir_driver = {
	.probe = aic_cir_probe,
	.remove = aic_cir_remove,
	.driver = {
		.name = AIC_IR_DEV,
		.of_match_table = aic_cir_match,
	},
};

module_platform_driver(aic_cir_driver);

MODULE_DESCRIPTION("ArtInChip CIR controller driver");
MODULE_AUTHOR("dwj <weijie.ding@artinchip.com>");
MODULE_LICENSE("GPL");

