// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 ArtInChip Technology Co., Ltd.
 * Author: Keliang Liu <keliang.liu@artinchip.com>
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/serial_8250.h>
#include <linux/serial_reg.h>
#include <linux/serial_core.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/reset.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#include "8250_artinchip.h"

int aic8250_dma_tx(struct uart_8250_port *p);

static inline struct aic8250_data *
			to_aic8250_data(struct aic8250_port_data *data)
{
	return container_of(data, struct aic8250_data, data);
}

static inline int aic8250_modify_msr(struct uart_port *p,
			int offset, int value)
{
	struct aic8250_data *d = to_aic8250_data(p->private_data);

	if (offset == UART_MSR) {
		value |= d->msr_mask_on;
		value &= ~d->msr_mask_off;
	}

	return value;
}

static void aic8250_force_idle(struct uart_port *p)
{
	struct uart_8250_port *up = up_to_u8250p(p);

	serial8250_clear_and_reinit_fifos(up);
	(void)p->serial_in(p, UART_RX);
}

static void aic8250_check_lcr(struct uart_port *p, int value)
{
	void __iomem *offset = p->membase + (UART_LCR << p->regshift);
	int tries = 1000;

	while (tries--) {
		unsigned int lcr = p->serial_in(p, UART_LCR);

		if ((value & ~UART_LCR_SPAR) == (lcr & ~UART_LCR_SPAR))
			return;

		aic8250_force_idle(p);

		if (p->iotype == UPIO_MEM32)
			writel(value, offset);
		else if (p->iotype == UPIO_MEM32BE)
			iowrite32be(value, offset);
		else
			writeb(value, offset);
	}
}

static void aic8250_serial_out32(struct uart_port *p, int offset, int value)
{
	struct aic8250_data *d = to_aic8250_data(p->private_data);

	writel(value, p->membase + (offset << p->regshift));

	if (offset == UART_LCR && !d->uart_16550_compatible)
		aic8250_check_lcr(p, value);
}

static unsigned int aic8250_serial_in32(struct uart_port *p, int offset)
{
	unsigned int value = readl(p->membase + (offset << p->regshift));

	return aic8250_modify_msr(p, offset, value);
}

static int aic8250_handle_irq(struct uart_port *p)
{
	struct aic8250_data *d = to_aic8250_data(p->private_data);
	unsigned int iir = p->serial_in(p, UART_IIR);
	unsigned int status;
	unsigned long flags;

	if (iir & AIC_IIR_SREMPTY_STATUS)
		d->tx_empty = 1;
	else
		d->tx_empty = 0;

	if ((iir & 0xf) == UART_IIR_RX_TIMEOUT) {
		spin_lock_irqsave(&p->lock, flags);
		status = p->serial_in(p, UART_LSR);

		if (!(status & (UART_LSR_DR | UART_LSR_BI)))
			(void)p->serial_in(p, UART_RX);

		spin_unlock_irqrestore(&p->lock, flags);
	}

	if (serial8250_handle_irq(p, iir))
		return 1;

	if ((iir & UART_IIR_BUSY) == UART_IIR_BUSY) {
		(void)p->serial_in(p, AIC_REG_UART_USR); /* Clear the USR */

		return 1;
	}

	return 0;
}

static void aic8250_do_pm(struct uart_port *port, unsigned int state,
			  unsigned int old)
{
	if (!state)
		pm_runtime_get_sync(port->dev);

	serial8250_do_pm(port, state, old);

	if (state)
		pm_runtime_put_sync_suspend(port->dev);
}

static void aic8250_set_termios(struct uart_port *p, struct ktermios *termios,
				struct ktermios *old)
{
	p->status &= ~UPSTAT_AUTOCTS;
	if (termios->c_cflag & CRTSCTS)
		p->status |= UPSTAT_AUTOCTS;

	serial8250_do_set_termios(p, termios, old);
}

static void aic8250_set_ldisc(struct uart_port *p, struct ktermios *termios)
{
	struct uart_8250_port *up = up_to_u8250p(p);
	unsigned int mcr = p->serial_in(p, UART_MCR);

	if (up->capabilities & UART_CAP_IRDA) {
		if (termios->c_line == N_IRDA)
			mcr |= AIC_UART_MCR_SIRE;
		else
			mcr &= ~AIC_UART_MCR_SIRE;

		p->serial_out(p, UART_MCR, mcr);
	}
	serial8250_do_set_ldisc(p, termios);
}

static int aic8250_rs485_config(struct uart_port *p,
				struct serial_rs485 *prs485)
{
	struct aic8250_data *d = to_aic8250_data(p->private_data);
	unsigned int mcr = p->serial_in(p, UART_MCR);
	unsigned int rs485 = p->serial_in(p, AIC_REG_UART_RS485);

	mcr &= AIC_UART_MCR_FUNC_MASK;
	if (prs485->flags & SER_RS485_ENABLED) {
		if (d->rs485simple)
			mcr |= AIC_UART_MCR_RS485S;
		else
			mcr |= AIC_UART_MCR_RS485;

		rs485 |= AIC_UART_RS485_RXBFA;
		rs485 |= AIC_UART_RS485_RXAFA;
		rs485 &= ~AIC_UART_RS485_CTL_MODE;
	} else {
		mcr = AIC_UART_MCR_UART;
		rs485 &= ~AIC_UART_RS485_RXBFA;
		rs485 &= ~AIC_UART_RS485_RXAFA;
	}

	p->serial_out(p, UART_MCR, mcr);
	p->serial_out(p, AIC_REG_UART_RS485, rs485);

	return 0;
}

static void aic8250_set_mctrl(struct uart_port *p, unsigned int mctrl)
{
	struct aic8250_data *d = to_aic8250_data(p->private_data);
	struct uart_8250_port *up = up_to_u8250p(p);
	unsigned char mcr;

	if (p->rs485.flags & SER_RS485_ENABLED) {
		if (serial8250_in_MCR(up) & UART_MCR_RTS)
			mctrl |= TIOCM_RTS;
		else
			mctrl &= ~TIOCM_RTS;
	}

	mcr = serial8250_TIOCM_to_MCR(mctrl);
	mcr = (mcr & AIC_UART_MCR_FUNC_MASK) | up->mcr_force | up->mcr;

	if (p->rs485.flags & SER_RS485_ENABLED) {
		if (d->rs485simple)
			mcr |= AIC_UART_MCR_RS485S;
		else
			mcr |= AIC_UART_MCR_RS485;
	}
	serial8250_out_MCR(up, mcr);
}

#ifdef CONFIG_SERIAL_8250_DMA
static bool aic8250_fallback_dma_filter(struct dma_chan *chan, void *param)
{
	return false;
}

static void aic8250_set_ier(struct uart_8250_port *up, bool enable)
{
	if (enable) {
		up->ier |= UART_IER_RLSI;
		up->ier |= UART_IER_RDI;
	} else {
		up->ier &= ~UART_IER_RLSI;
		up->ier &= ~UART_IER_RDI;
	}

	up->port.serial_out(&up->port, UART_IER, up->ier);
}

static void aic8250_dma_tx_complete(void *param)
{
	struct uart_8250_port	*p = param;
	struct uart_8250_dma	*dma = p->dma;
	struct circ_buf		*xmit = &p->port.state->xmit;
	unsigned long	flags;
	int		ret;

	dma_sync_single_for_cpu(dma->txchan->device->dev, dma->tx_addr,
				UART_XMIT_SIZE, DMA_TO_DEVICE);

	spin_lock_irqsave(&p->port.lock, flags);
	p->port.icount.tx += dma->tx_size;

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&p->port);

	dma->tx_running = 0;
	ret = aic8250_dma_tx(p);
	if (ret)
		serial8250_set_THRI(p);

	spin_unlock_irqrestore(&p->port.lock, flags);
}

static void aic8250_dma_rx_complete(void *param)
{
	struct uart_8250_port	*p = param;
	struct uart_8250_dma	*dma = p->dma;
	struct tty_port		*tty_port = &p->port.state->port;
	struct dma_tx_state	state;
	int			count;

	dma->rx_running = 0;
	dmaengine_tx_status(dma->rxchan, dma->rx_cookie, &state);

	count = dma->rx_size - state.residue;

	tty_insert_flip_string(tty_port, dma->rx_buf, count);
	p->port.icount.rx += count;

	tty_flip_buffer_push(tty_port);
	aic8250_set_ier(p, true);
}

int aic8250_dma_tx(struct uart_8250_port *p)
{
	struct uart_8250_dma		*dma = p->dma;
	struct circ_buf			*xmit = &p->port.state->xmit;
	struct dma_async_tx_descriptor	*desc;
	int ret;

	if (dma->tx_running)
		return 0;

	if (uart_tx_stopped(&p->port) || uart_circ_empty(xmit)) {
		/* We have been called from __dma_tx_complete() */
		serial8250_rpm_put_tx(p);
		return 0;
	}

	dma->tx_size = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
	memcpy(dma->tx_buf, xmit->buf + xmit->tail, dma->tx_size);
	xmit->tail += dma->tx_size;
	xmit->tail &= UART_XMIT_SIZE - 1;

	desc = dmaengine_prep_slave_single(dma->txchan,
					   dma->tx_addr,
					   dma->tx_size, DMA_MEM_TO_DEV,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		ret = -EBUSY;
		goto err;
	}

	dma->tx_running = 1;
	desc->callback = aic8250_dma_tx_complete;
	desc->callback_param = p;

	dma->tx_cookie = dmaengine_submit(desc);

	dma_sync_single_for_device(dma->txchan->device->dev, dma->tx_addr,
				   UART_XMIT_SIZE, DMA_TO_DEVICE);

	dma_async_issue_pending(dma->txchan);
	if (dma->tx_err) {
		dma->tx_err = 0;
		serial8250_clear_THRI(p);
	}
	return 0;
err:
	dma->tx_err = 1;
	return ret;
}

int aic8250_dma_rx(struct uart_8250_port *p)
{
	struct uart_8250_dma		*dma = p->dma;
	struct dma_async_tx_descriptor	*desc;

	if (dma->rx_running)
		return 0;

	aic8250_set_ier(p, false);
	dma->rx_size = p->port.serial_in(&p->port, AIC_REG_UART_RFL);

	desc = dmaengine_prep_slave_single(dma->rxchan, dma->rx_addr,
					   dma->rx_size, DMA_DEV_TO_MEM,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc)
		return -EBUSY;

	dma->rx_running = 1;
	desc->callback = aic8250_dma_rx_complete;
	desc->callback_param = p;

	dma->rx_cookie = dmaengine_submit(desc);

	dma_async_issue_pending(dma->rxchan);

	return 0;
}
#endif

static void aic8250_init_special_reg(struct uart_port *p)
{
	unsigned int ier = p->serial_in(p, UART_IER);
	ier |= AIC_IER_SREMPTY_ENABLE;
	ier |= AIC_IER_RS485_INT_EN;

	p->serial_out(p, UART_IER, ier);
	p->serial_out(p, AIC_REG_UART_HSK, AIC_HSK_HAND_SHAKE);
}

static void aic8250_apply_quirks(struct device *dev, struct uart_port *p,
				 struct aic8250_data *data)
{
	struct device_node *np = p->dev->of_node;
	int id;

	if (!np) {
		dev_warn(dev, "no of node get from dts, warnning!\n");
		return;
	}

	id = of_alias_get_id(np, "serial");
	if (id >= 0)
		p->line = id;

	if (device_property_read_bool(dev, "aic,rs485-compact-io-mode"))
		data->rs485simple = 1;

	uart_get_rs485_mode(p);
}

static int aic8250_probe(struct platform_device *pdev)
{
	struct uart_8250_port uart = {}, *up = &uart;
	struct resource *regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct uart_port *p = &up->port;
	struct device *dev = &pdev->dev;
	struct aic8250_data *data;
	int irq;
	int err;
	u32 val;

	if (!regs) {
		dev_err(dev, "no registers defined\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "can not get irq\n");
		return irq;
	}

	spin_lock_init(&p->lock);
	p->dev = dev;
	p->mapbase = regs->start;
	p->type = PORT_16550A;
	p->flags = UPF_SHARE_IRQ | UPF_FIXED_PORT | UPF_FIXED_TYPE;
	p->iotype = UPIO_MEM32;
	p->regshift = AIC_REG_SHIFT_DEFAULT;
	p->irq = irq;
	p->handle_irq = aic8250_handle_irq;
	p->pm = aic8250_do_pm;
	p->serial_in = aic8250_serial_in32;
	p->serial_out = aic8250_serial_out32;
	p->set_ldisc = aic8250_set_ldisc;
	p->set_termios = aic8250_set_termios;
	p->rs485_config = aic8250_rs485_config;
	p->set_mctrl = aic8250_set_mctrl;

	p->membase = devm_ioremap(dev, regs->start, resource_size(regs));
	if (!p->membase) {
		dev_err(dev, "io remap failed\n");
		return -ENOMEM;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	p->private_data = &data->data;

	err = device_property_read_u32(dev, "reg-shift", &val);
	if (!err)
		p->regshift = val;

	data->clk = devm_clk_get_optional(dev, NULL);
	if (IS_ERR(data->clk)) {
		dev_err(dev, "can not get clk\n");
		return PTR_ERR(data->clk);
	}

	err = device_property_read_u32(dev, "clock-frequency", &p->uartclk);
	if (err)
		p->uartclk = AICUART_DEFAULT_CLOCK;

	clk_set_rate(data->clk, p->uartclk);
	clk_prepare_enable(data->clk);

	data->rst = devm_reset_control_get_optional_exclusive(dev, NULL);
	if (IS_ERR(data->rst)) {
		err = PTR_ERR(data->rst);
		dev_err(dev, "can not get rst\n");
		goto err_clk;
	}
	reset_control_deassert(data->rst);

	aic8250_apply_quirks(dev, p, data);

	aic8250_init_special_reg(p);
#ifdef CONFIG_SERIAL_8250_DMA
	p->fifosize = AICUART_FIFO_SIZE;
	data->data.dma.fn = aic8250_fallback_dma_filter;
	data->data.dma.rxconf.src_maxburst = AICUART_MAX_BRUST;
	data->data.dma.txconf.dst_maxburst = AICUART_MAX_BRUST;
	data->data.dma.tx_dma = aic8250_dma_tx;
	data->data.dma.rx_dma = aic8250_dma_rx;
	up->dma = &data->data.dma;
	up->capabilities |= UART_CAP_FIFO;
	up->tx_loadsz = 16;
#else
	p->fifosize = 0;
#endif
	data->data.line = serial8250_register_8250_port(up);
	if (data->data.line < 0) {
		err = data->data.line;
		dev_err(dev, "port register failed\n");
		goto err_reset;
	}

	platform_set_drvdata(pdev, data);

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;

err_reset:
	reset_control_assert(data->rst);

err_clk:
	clk_disable_unprepare(data->clk);

	return err;
}

static int aic8250_remove(struct platform_device *pdev)
{
	struct aic8250_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	pm_runtime_get_sync(dev);

	serial8250_unregister_port(data->data.line);

	reset_control_assert(data->rst);

	clk_disable_unprepare(data->clk);

	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int aic8250_suspend(struct device *dev)
{
	struct clk_hw *uart_clk_hw;
	struct aic8250_data *data = dev_get_drvdata(dev);

	serial8250_suspend_port(data->data.line);

	uart_clk_hw = __clk_get_hw(data->clk);
	if (clk_hw_is_prepared(uart_clk_hw))
		clk_disable_unprepare(data->clk);
	return 0;
}

static int aic8250_resume(struct device *dev)
{
	struct clk_hw *uart_clk_hw;
	struct aic8250_data *data = dev_get_drvdata(dev);

	uart_clk_hw = __clk_get_hw(data->clk);
	if (!clk_hw_is_prepared(uart_clk_hw))
		clk_prepare_enable(data->clk);

	serial8250_resume_port(data->data.line);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static int aic8250_runtime_suspend(struct device *dev)
{
	struct aic8250_data *data = dev_get_drvdata(dev);

	clk_disable_unprepare(data->clk);

	return 0;
}

static int aic8250_runtime_resume(struct device *dev)
{
	struct aic8250_data *data = dev_get_drvdata(dev);

	clk_prepare_enable(data->clk);

	return 0;
}
#endif

static const struct dev_pm_ops aic8250_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	SET_SYSTEM_SLEEP_PM_OPS(aic8250_suspend, aic8250_resume)
#endif
#ifdef CONFIG_PM
		SET_RUNTIME_PM_OPS(aic8250_runtime_suspend,
				   aic8250_runtime_resume, NULL)
#endif
};

static const struct of_device_id aic8250_of_match[] = {
	{ .compatible = "artinchip,aic-uart-v1.0" },
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, aic8250_of_match);

static struct platform_driver aic8250_platform_driver = {
	.driver = {
		.name	= AICUART_DRIVER_NAME,
		.pm		= &aic8250_pm_ops,
		.of_match_table	= aic8250_of_match,
	},
	.probe			= aic8250_probe,
	.remove			= aic8250_remove,
};

module_platform_driver(aic8250_platform_driver);

MODULE_AUTHOR("Keliang Liu");
MODULE_DESCRIPTION("ArtInChip 16550 serial Driver");
MODULE_ALIAS("platform:" AICUART_DRIVER_NAME);
MODULE_LICENSE("GPL");
