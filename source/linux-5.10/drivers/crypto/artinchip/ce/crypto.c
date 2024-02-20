// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 *
 * Wu Dehuang <dehuang.wu@artinchip.com>
 */

#include <linux/clk.h>
#include <linux/crypto.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <crypto/aes.h>
#include <crypto/internal/des.h>
#include <crypto/engine.h>
#include <crypto/scatterwalk.h>
#include <crypto/internal/skcipher.h>
#include <crypto/internal/rng.h>
#include "crypto.h"

#define DRIVER_NAME			"aic-crypto"

static irqreturn_t aic_crypto_irq_thread(int irq, void *arg)
{
	struct aic_crypto_dev *ce_dev = arg;
	u32 mask;

	mask = BIT(SK_ALG_ACCELERATOR) | BIT(AK_ALG_ACCELERATOR) |
	       BIT(HASH_ALG_ACCELERATOR);
loop:
	if (ce_dev->irq_status & BIT(SK_ALG_ACCELERATOR)) {
		aic_skcipher_handle_irq(ce_dev);
		ce_dev->irq_status &= ~BIT(SK_ALG_ACCELERATOR);
		ce_dev->err_status &= (0xff << (8 * SK_ALG_ACCELERATOR));
	}
	if (ce_dev->irq_status & BIT(AK_ALG_ACCELERATOR)) {
		aic_akcipher_handle_irq(ce_dev);
		ce_dev->irq_status &= ~BIT(AK_ALG_ACCELERATOR);
		ce_dev->err_status &= (0xff << (8 * AK_ALG_ACCELERATOR));
	}
	if (ce_dev->irq_status & BIT(HASH_ALG_ACCELERATOR)) {
		aic_hash_handle_irq(ce_dev);
		ce_dev->irq_status &= ~BIT(HASH_ALG_ACCELERATOR);
		ce_dev->err_status &= (0xff << (8 * HASH_ALG_ACCELERATOR));
	}

	if (ce_dev->irq_status & mask)
		goto loop;

	return IRQ_HANDLED;
}

static irqreturn_t aic_crypto_irq_handler(int irq, void *arg)
{
	struct aic_crypto_dev *ce_dev = arg;
	u32 ints, errs;

	ints = readl(ce_dev->base + CE_REG_ISR);
	errs = readl(ce_dev->base + CE_REG_TER);
	ce_dev->irq_status |= ints;
	ce_dev->err_status |= errs;

	writel(ints, ce_dev->base + CE_REG_ISR);
	return IRQ_WAKE_THREAD;
}

bool aic_crypto_is_ce_avail(struct aic_crypto_dev *dev)
{
	u32 val;

	val = readl(dev->base + CE_REG_TCR);
	return ((val & (0x1UL << TASK_LOAD_BIT_OFFSET)) == 0);
}

bool aic_crypto_is_accel_avail(struct aic_crypto_dev *dev, int accel)
{
	return true;
}

bool aic_crypto_check_task_done(struct aic_crypto_dev *dev, int accel)
{
	u32 val;

	val = readl(dev->base + CE_REG_ISR);
	if (val & BIT(accel))
		return true;

	return false;
}

void aic_crypto_pending_clear(struct aic_crypto_dev *dev, int accel)
{
	writel(BIT(accel), dev->base + CE_REG_ISR);
}

void aic_crypto_irq_enable(struct aic_crypto_dev *ce_dev, int accel)
{
	u32 rval;

	rval = readl(ce_dev->base + CE_REG_ICR);
	rval |= BIT(accel);
	writel(rval, ce_dev->base + CE_REG_ICR);
}

void aic_crypto_irq_disable(struct aic_crypto_dev *ce_dev, int accel)
{
	u32 rval;

	rval = readl(ce_dev->base + CE_REG_ICR);
	rval &= ~(BIT(accel));
	writel(rval, ce_dev->base + CE_REG_ICR);
}

void aic_crypto_hardware_reset(struct aic_crypto_dev *dev)
{
	if (!IS_ERR(dev->reset)) {
		reset_control_assert(dev->reset);
		udelay(2);
		reset_control_deassert(dev->reset);
		udelay(2);
	}
}

/*
 * Swap byte order in-place
 */
int aic_crypto_bignum_byteswap(u8 *bn, u32 len)
{
	u32 i, j;
	u8 val;

	if (len == 0)
		return -1;

	i = 0;
	j = len - 1;
	while (i < j) {
		val = bn[i];
		bn[i] = bn[j];
		bn[j] = val;
		i++;
		j--;
	}
	return 0;
}

int aic_crypto_bignum_le2be(u8 *src, u32 slen, u8 *dst, u32 dlen)
{
	int i;

	memset(dst, 0, dlen);
	for (i = 0; i < slen && i < dlen; i++)
		dst[dlen - 1 - i] = src[i];

	return 0;
}

int aic_crypto_bignum_be2le(u8 *src, u32 slen, u8 *dst, u32 dlen)
{
	int i;

	memset(dst, 0, dlen);
	for (i = 0; i < slen && i < dlen; i++)
		dst[i] = src[slen - 1 - i];

	return 0;
}

int aic_crypto_enqueue_task(struct aic_crypto_dev *dev, u32 algo,
			    dma_addr_t phy_task)
{
	u32 val;

	writel(cpu_to_le32(phy_task), dev->base + CE_REG_TAR);

	val = (algo << TASK_ALG_BIT_OFFSET) | (0x1UL << TASK_LOAD_BIT_OFFSET);
	writel(val, dev->base + CE_REG_TCR);

	return 0;
}

void aic_crypto_sg_copy(void *buf, struct scatterlist *sg, size_t len, int out)
{
	struct scatter_walk walk;

	if (!len)
		return;

	scatterwalk_start(&walk, sg);
	scatterwalk_advance(&walk, 0);
	scatterwalk_copychunks(buf, &walk, len, out);
	scatterwalk_done(&walk, out, 0);
}

void aic_crypto_dump_reg(struct aic_crypto_dev *dev)
{
	pr_err("0x08 ICR 0x%08x\n", readl(dev->base + CE_REG_ICR));
	pr_err("0x0c ISR 0x%08x\n", readl(dev->base + CE_REG_ISR));
	pr_err("0x00 TAR 0x%08x\n", readl(dev->base + CE_REG_TAR));
	pr_err("0x10 TCR 0x%08x\n", readl(dev->base + CE_REG_TCR));
	pr_err("0x14 TSR 0x%08x\n", readl(dev->base + CE_REG_TSR));
	pr_err("0x18 TER 0x%08x\n", readl(dev->base + CE_REG_TER));
}

#define is_aes(alg) (((alg) & 0xF0) == 0)
#define is_des(alg) ((((alg) & 0xF0) == 0x10) || (((alg) & 0xF0) == 0x20))
#define is_hash(alg) (((alg) & 0xF0) == 0x40)

void aic_crypto_dump_task(struct task_desc *task, int len)
{
	u32 i, count;

	count = len / sizeof(struct task_desc);
	for (i = 0; i < count; i++) {
		pr_err("task:               0x%08lx\n", (unsigned long)task);
		if (is_aes(task->alg.alg_tag)) {
			pr_err("  alg.alg_tag:      %08x\n",
			       task->alg.aes_ecb.alg_tag);
			pr_err("  alg.direction:    %u\n",
			       task->alg.aes_ecb.direction);
			pr_err("  alg.key_siz:      %u\n",
			       task->alg.aes_ecb.key_siz);
			pr_err("  alg.key_src:      %u\n",
			       task->alg.aes_ecb.key_src);
			pr_err("  alg.key_addr:     %08x\n",
			       task->alg.aes_ecb.key_addr);
			if (task->alg.alg_tag == ALG_TAG_AES_CBC)
				pr_err("  alg.iv_addr:      %08x\n",
				       task->alg.aes_cbc.iv_addr);

			if (task->alg.alg_tag == ALG_TAG_AES_CTR) {
				pr_err("  alg.ctr_in:       %08x\n",
				       task->alg.aes_ctr.ctr_in_addr);
				pr_err("  alg.ctr_out:      %08x\n",
				       task->alg.aes_ctr.ctr_out_addr);
			}
			if (task->alg.alg_tag == ALG_TAG_AES_CTS)
				pr_err("  alg.iv_addr:      %08x\n",
				       task->alg.aes_cts.iv_addr);

			if (task->alg.alg_tag == ALG_TAG_AES_XTS)
				pr_err("  alg.tweak_addr:   %08x\n",
				       task->alg.aes_xts.tweak_addr);

			pr_err("  data.in_addr      %08x\n",
			       task->data.in_addr);
			pr_err("  data.in_len       %u\n",
			       task->data.in_len);
			pr_err("  data.out_addr     %08x\n",
			       task->data.out_addr);
			pr_err("  data.out_len      %u\n",
			       task->data.out_len);
		}
		if (is_des(task->alg.alg_tag)) {
			pr_err("  alg.alg_tag:      %08x\n",
			       task->alg.des_ecb.alg_tag);
			pr_err("  alg.direction:    %u\n",
			       task->alg.des_ecb.direction);
			pr_err("  alg.key_siz:      %u\n",
			       task->alg.des_ecb.key_siz);
			pr_err("  alg.key_src:      %u\n",
			       task->alg.des_ecb.key_src);
			pr_err("  alg.key_addr:     %08x\n",
			       task->alg.des_ecb.key_addr);
			if ((task->alg.alg_tag == ALG_TAG_DES_CBC) ||
			    (task->alg.alg_tag == ALG_TAG_TDES_CBC))
				pr_err("  alg.iv_addr:      %08x\n",
				       task->alg.des_cbc.iv_addr);

			pr_err("  data.in_addr      %08x\n",
			       task->data.in_addr);
			pr_err("  data.in_len       %u\n",
			       task->data.in_len);
			pr_err("  data.out_addr     %08x\n",
			       task->data.out_addr);
			pr_err("  data.out_len      %u\n",
			       task->data.out_len);
		}
		if (task->alg.alg_tag == ALG_TAG_RSA) {
			pr_err("  alg.alg_tag:      %08x\n",
			       task->alg.rsa.alg_tag);
			pr_err("  alg.op_siz:       %u\n",
			       task->alg.rsa.op_siz);
			pr_err("  alg.m_addr:       %08x\n",
			       task->alg.rsa.m_addr);
			pr_err("  alg.d_e_addr:     %08x\n",
			       task->alg.rsa.d_e_addr);

			pr_err("  data.in_addr      %08x\n",
			       task->data.in_addr);
			pr_err("  data.in_len       %u\n",
			       task->data.in_len);
			pr_err("  data.out_addr     %08x\n",
			       task->data.out_addr);
			pr_err("  data.out_len      %u\n",
			       task->data.out_len);
		}
		if (is_hash(task->alg.alg_tag)) {
			pr_err("  alg.alg_tag:      %08x\n",
			       task->alg.hmac.alg_tag);
			pr_err("  alg.iv_mode:      %u\n",
			       task->alg.hmac.iv_mode);
			pr_err("  alg.iv_addr:      %08x\n",
			       task->alg.hmac.iv_addr);
			pr_err("  alg.key_addr:     %08x\n",
			       task->alg.hmac.hmac_key_addr);

			pr_err("  data.first        %u\n",
			       task->data.first_flag);
			pr_err("  data.last         %u\n",
			       task->data.last_flag);
			pr_err("  data.total_byte   %u\n",
			       task->data.total_bytelen);
			pr_err("  data.in_addr      %08x\n",
			       task->data.in_addr);
			pr_err("  data.in_len       %u\n",
			       task->data.in_len);
			pr_err("  data.out_addr     %08x\n",
			       task->data.out_addr);
			pr_err("  data.out_len      %u\n",
			       task->data.out_len);
		}
		pr_err("  next:             %08x\n\n", task->next);
		task++;
	}
}

static int aic_crypto_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aic_crypto_dev *ce_dev;
	int irq, ret;

	ce_dev = devm_kzalloc(dev, sizeof(*ce_dev), GFP_KERNEL);
	if (!ce_dev)
		return -ENOMEM;

	ce_dev->dev = dev;
	ce_dev->task_count = 0;

	ce_dev->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ce_dev->base))
		return PTR_ERR(ce_dev->base);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_threaded_irq(dev, irq, aic_crypto_irq_handler,
					aic_crypto_irq_thread, IRQF_ONESHOT,
					dev_name(dev), ce_dev);
	if (ret) {
		dev_err(dev, "Request IRQ failed.\n");
		return ret;
	}

	ce_dev->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(ce_dev->clk)) {
		dev_err(dev, "Get clock failed.\n");
		return PTR_ERR(ce_dev->clk);
	}

	ret = of_property_read_u32(dev->of_node, "clock-rate", &ce_dev->clk_rate);
	if (ret) {
		dev_err(dev, "Can't parse clock-rate\n");
		return ret;
	}

	clk_set_rate(ce_dev->clk, ce_dev->clk_rate);

	ret = clk_prepare_enable(ce_dev->clk);
	if (ret) {
		dev_err(ce_dev->dev, "Failed to enable clock\n");
		return ret;
	}

	ce_dev->reset = devm_reset_control_get(dev, NULL);
	if (!IS_ERR(ce_dev->reset)) {
		reset_control_assert(ce_dev->reset);
		udelay(2);
		reset_control_deassert(ce_dev->reset);
	}

	pm_runtime_set_autosuspend_delay(dev, 1000);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	platform_set_drvdata(pdev, ce_dev);

	mutex_init(&ce_dev->ssram_lock);

	ret = aic_crypto_skcipher_accelerator_init(ce_dev);
	if (ret) {
		dev_err(ce_dev->dev, "Failed to init skcipher accelerator\n");
		return ret;
	}
	ret = aic_crypto_akcipher_accelerator_init(ce_dev);
	if (ret) {
		dev_err(ce_dev->dev, "Failed to init akcipher accelerator\n");
		return ret;
	}
	ret = aic_crypto_hash_accelerator_init(ce_dev);
	if (ret) {
		dev_err(ce_dev->dev, "Failed to init hash accelerator\n");
		return ret;
	}

	return ret;
}

static int aic_crypto_remove(struct platform_device *pdev)
{
	struct aic_crypto_dev *ce_dev = platform_get_drvdata(pdev);

	if (!ce_dev)
		return -ENODEV;

	aic_crypto_skcipher_accelerator_exit(ce_dev);
	aic_crypto_akcipher_accelerator_exit(ce_dev);
	aic_crypto_skcipher_accelerator_exit(ce_dev);

	clk_disable_unprepare(ce_dev->clk);

	return 0;
}

static const struct of_device_id aic_dt_ids[] = {
	{ .compatible = "artinchip,aic-crypto-v1.0" },
	{},
};

#ifdef CONFIG_PM
static int aic_crypto_runtime_suspend(struct device *dev)
{
	struct aic_crypto_dev *ce_dev = dev_get_drvdata(dev);

	clk_disable_unprepare(ce_dev->clk);
	return 0;
}

static int aic_crypto_runtime_resume(struct device *dev)
{
	struct aic_crypto_dev *ce_dev = dev_get_drvdata(dev);

	clk_prepare_enable(ce_dev->clk);
	return 0;
}

static const struct dev_pm_ops aic_crypto_pm_ops = {
	SET_RUNTIME_PM_OPS(
			aic_crypto_runtime_suspend,
			aic_crypto_runtime_resume,
			NULL) };

#define AIC_CRYPTO_DEV_PM_OPS (&aic_crypto_pm_ops)
#else
#define AIC_CRYPTO_DEV_PM_OPS NULL
#endif

MODULE_DEVICE_TABLE(of, aic_dt_ids);

static struct platform_driver aic_crypto_driver = {
	.probe  = aic_crypto_probe,
	.remove = aic_crypto_remove,
	.driver = {
		.name           = DRIVER_NAME,
		.of_match_table = aic_dt_ids,
		.pm = AIC_CRYPTO_DEV_PM_OPS,
	},
};

module_platform_driver(aic_crypto_driver);

MODULE_AUTHOR("Wu Dehuang <dehuang.wu@artinchip.com>");
MODULE_DESCRIPTION("Artinchip crypto engine driver");
MODULE_LICENSE("GPL");
