// SPDX-License-Identifier: GPL-2.0-only
/*
 * SPI Bus Encryption driver for Artinchip SPI Enc device
 *
 * Copyright (c) 2021, Artinchip Technology Co., Ltd
 * Dehuang Wu <dehuang.wu@artinchip.com>
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <crypto/aes.h>
#include <crypto/algapi.h>
#include <crypto/internal/skcipher.h>
#include <linux/aic_spienc.h>

#define DRIVER_NAME               "aic-spienc"

#define SPIE_REG_CTL              0x00
#define SPIE_REG_ICR              0x04
#define SPIE_REG_ISR              0x08
#define SPIE_REG_KCNT             0x0C
#define SPIE_REG_OCNT             0x10
#define SPIE_REG_ADDR             0x14
#define SPIE_REG_TWEAK            0x18
#define SPIE_REG_CPOS             0x1C
#define SPIE_REG_CLEN             0x20
#define SPIE_REG_VER              0xFFC

#define SPIE_START_OFF            0
#define SPIE_SPI_SEL_OFF          12

#define SPIE_START_MSK            (0x1 << SPIE_START_OFF)
#define SPIE_SPI_SEL_MSK          (0x3 << SPIE_SPI_SEL_OFF)

#define SPIE_INTR_KEY_GEN_MSK     (1 << 0)
#define SPIE_INTR_ENC_DEC_FIN_MSK (1 << 1)
#define SPIE_INTR_ALL_EMP_MSK     (1 << 2)
#define SPIE_INTR_HALF_EMP_MSK    (1 << 3)
#define SPIE_INTR_KEY_UDF_MSK     (1 << 4)
#define SPIE_INTR_KEY_OVF_MSK     (1 << 5)
#define SPIE_INTR_ALL_MSK         (0x3F)

#define SPI_BUS_0                 0
#define SPI_BUS_1                 1
#define SPI_BUS_INVAL             0xFF

struct aic_spienc_drvdata {
	struct attribute_group attrs;
	struct device *dev;
	void __iomem *base;
	struct clk *clk;
	struct skcipher_request *req;
	u32 tweak; /* Tweak value for hardware to generate counter */
	u32 irq_sts;
#ifdef CONFIG_ARTINCHIP_SPIENC_DEBUG
	u32 bypass;
#endif
};

struct aic_spienc_ctx {
	struct aic_spienc_drvdata *drvdata;
};

struct aic_spienc_alg {
	struct skcipher_alg alg;
	struct aic_spienc_drvdata *drvdata;
};

static DEFINE_SPINLOCK(user_lock);

static int aic_spienc_alg_init(struct crypto_skcipher *tfm);
static int aic_spienc_encrypt(struct skcipher_request *req);
static int aic_spienc_decrypt(struct skcipher_request *req);

static struct aic_spienc_alg spienc_alg = {
	.alg = {
		.base.cra_name = "ctr(aes)",
		.base.cra_driver_name = "ctr-aes-spienc-aic",
		.base.cra_priority = 200,
		.base.cra_flags = CRYPTO_ALG_ASYNC |
				  CRYPTO_ALG_ALLOCATES_MEMORY |
				  CRYPTO_ALG_KERN_DRIVER_ONLY,
		.base.cra_blocksize = 1,
		.base.cra_ctxsize = sizeof(struct aic_spienc_ctx),
		.base.cra_alignmask = 0xf,
		.base.cra_module = THIS_MODULE,
		.init = aic_spienc_alg_init,
		.decrypt = aic_spienc_decrypt,
		.encrypt = aic_spienc_encrypt,
		.ivsize = AES_BLOCK_SIZE,
	},
};

static int aic_spienc_alg_init(struct crypto_skcipher *tfm)
{
	struct aic_spienc_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct skcipher_alg *skalg = crypto_skcipher_alg(tfm);
	struct aic_spienc_alg *aicalg;

	memset(ctx, 0, sizeof(*ctx));
	aicalg = container_of(skalg, struct aic_spienc_alg, alg);
	ctx->drvdata = aicalg->drvdata;

	return 0;
}

static irqreturn_t aic_spienc_irq_thread(int irq, void *arg)
{
	struct aic_spienc_drvdata *drvdata = arg;
	struct crypto_async_request *base;
	int err = 0;
	u32 val = 0;

	if (drvdata->irq_sts & SPIE_INTR_ENC_DEC_FIN_MSK) {
		if (drvdata->irq_sts & SPIE_INTR_ALL_EMP_MSK) {
			pr_debug("All FF\n");
			err = AIC_SPIENC_ALL_FF;
		}

		pr_debug("Stop it.\n");
		/* Stop it */
		val = readl((drvdata->base + SPIE_REG_CTL));
		val &= ~SPIE_START_MSK;
		writel(val, (drvdata->base + SPIE_REG_CTL));

		/* Clear interrupts */
		drvdata->irq_sts = 0;
		base = &drvdata->req->base;
		base->complete(base, err);
	}

	return IRQ_HANDLED;
}

static irqreturn_t aic_spienc_irq_handler(int irq, void *arg)
{
	struct aic_spienc_drvdata *drvdata = arg;
	u32 sts;

	sts = readl(drvdata->base + SPIE_REG_ISR);

	writel(sts, (drvdata->base + SPIE_REG_ISR));
	drvdata->irq_sts |= sts;
	return IRQ_WAKE_THREAD;
}

static int aic_spienc_attach_bus(struct aic_spienc_drvdata *drvdata, u32 bus)
{
	u32 val;

	val = readl(drvdata->base + SPIE_REG_CTL);
	val &= ~SPIE_SPI_SEL_MSK;

	/* Attach SPI Bus */
	switch (bus) {
	case SPI_BUS_0:
		val |= (1 << SPIE_SPI_SEL_OFF);
		writel(val, (drvdata->base + SPIE_REG_CTL));
		pr_debug("Attach SPI 0.\n");
		break;
	case SPI_BUS_1:
		val |= (2 << SPIE_SPI_SEL_OFF);
		writel(val, (drvdata->base + SPIE_REG_CTL));
		pr_debug("Attach SPI 1.\n");
		break;
	case SPI_BUS_INVAL:
		/* Clear SPI SEL to zero. */
		val |= (0 << SPIE_SPI_SEL_OFF);
		writel(val, (drvdata->base + SPIE_REG_CTL));
		pr_debug("Clear attach SPI.\n");
		break;
	default:
		val |= (0 << SPIE_SPI_SEL_OFF);
		writel(val, (drvdata->base + SPIE_REG_CTL));
		dev_err(drvdata->dev, "Wrong SPI Controller ID In DTS\n");
		return -EINVAL;
	}

	return 0;
}

static int aic_spienc_xcrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm;
	struct aic_spienc_ctx *ctx;
	struct aic_spienc_drvdata *drvdata;
	struct aic_spienc_iv *ivinfo;
	u32 tweak, clen, val;
	int ret;

	tfm = crypto_skcipher_reqtfm(req);
	ctx = crypto_skcipher_ctx(tfm);
	drvdata = ctx->drvdata;

	if (!drvdata) {
		pr_err("drvdata is NULL.\n");
		return -ENODEV;
	}

	ret = -EINPROGRESS;

	spin_lock(&user_lock);
#ifdef CONFIG_ARTINCHIP_SPIENC_DEBUG
	if (drvdata->bypass) {
		struct crypto_async_request *base;
		base = &drvdata->req->base;
		base->complete(base, 0);
		goto unlock;
	}
#endif
	ivinfo = (struct aic_spienc_iv *)req->iv;

	if (aic_spienc_attach_bus(drvdata, ivinfo->spi_id)) {
		dev_err(drvdata->dev, "Failed to attach bus.\n");
		ret = -EINVAL;
		goto unlock;
	}

	clen = (u32)req->cryptlen;
	tweak = drvdata->tweak;
	if (ivinfo->tweak)
		tweak = ivinfo->tweak;

	writel(SPIE_INTR_ALL_MSK, (drvdata->base + SPIE_REG_ISR));
	writel(SPIE_INTR_ALL_MSK, (drvdata->base + SPIE_REG_ICR));

	val = readl((drvdata->base + SPIE_REG_CTL));

	/* Ensure it is stopped */
	val &= ~SPIE_START_MSK;
	writel(val, (drvdata->base + SPIE_REG_CTL));

	/* Setup parameters */
	writel(ivinfo->addr, (drvdata->base + SPIE_REG_ADDR));
	writel(ivinfo->cpos, (drvdata->base + SPIE_REG_CPOS));
	writel(clen, (drvdata->base + SPIE_REG_CLEN));
	writel(tweak, (drvdata->base + SPIE_REG_TWEAK));

	/* Start it */
	val = readl((drvdata->base + SPIE_REG_CTL));
	val |= SPIE_START_MSK;
	writel(val, (drvdata->base + SPIE_REG_CTL));

	drvdata->req = req;
unlock:
	spin_unlock(&user_lock);

	return ret;
}

static int aic_spienc_encrypt(struct skcipher_request *req)
{
	return aic_spienc_xcrypt(req);
}

static int aic_spienc_decrypt(struct skcipher_request *req)
{
	return aic_spienc_xcrypt(req);
}

static ssize_t status_show(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aic_spienc_drvdata *drvdata = platform_get_drvdata(pdev);
	ssize_t ret;

	spin_lock(&user_lock);
	ret = sprintf(buf,
			"SPI ENC :\n"
			"   CTL(0x%03X) = 0x%08X\n"
			"   ICR(0x%03X) = 0x%08X\n"
			"   ISR(0x%03X) = 0x%08X\n"
			"  KCNT(0x%03X) = 0x%08X\n"
			"  OCNT(0x%03X) = 0x%08X\n"
			"  ADDR(0x%03X) = 0x%08X\n"
			" TWEAK(0x%03X) = 0x%08X\n"
			"  CPOS(0x%03X) = 0x%08X\n"
			"  CLEN(0x%03X) = 0x%08X\n"
			"   VER(0x%03X) = 0x%08X\n",
			SPIE_REG_CTL, readl(drvdata->base + SPIE_REG_CTL),
			SPIE_REG_ICR, readl(drvdata->base + SPIE_REG_ICR),
			SPIE_REG_ISR, readl(drvdata->base + SPIE_REG_ISR),
			SPIE_REG_KCNT, readl(drvdata->base + SPIE_REG_KCNT),
			SPIE_REG_OCNT, readl(drvdata->base + SPIE_REG_OCNT),
			SPIE_REG_ADDR, readl(drvdata->base + SPIE_REG_ADDR),
			SPIE_REG_TWEAK, readl(drvdata->base + SPIE_REG_TWEAK),
			SPIE_REG_CPOS, readl(drvdata->base + SPIE_REG_CPOS),
			SPIE_REG_CLEN, readl(drvdata->base + SPIE_REG_CLEN),
			SPIE_REG_VER, readl(drvdata->base + SPIE_REG_VER)
			);
	spin_unlock(&user_lock);
	return ret;
}
static DEVICE_ATTR_RO(status);

#ifdef CONFIG_ARTINCHIP_SPIENC_DEBUG
static ssize_t bypass_show(struct device *dev,
			     struct device_attribute *devattr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aic_spienc_drvdata *drvdata = platform_get_drvdata(pdev);
	ssize_t ret;

	spin_lock(&user_lock);
	ret = sprintf(buf, "bypass = %d\n", drvdata->bypass);
	spin_unlock(&user_lock);
	return ret;
}

static ssize_t bypass_store(struct device *dev,
			      struct device_attribute *devattr, const char *buf,
			      size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aic_spienc_drvdata *drvdata = platform_get_drvdata(pdev);
	unsigned long val = 0;
	int ret;

	ret = kstrtoul(buf, 0, &val);
	if (ret) {
		dev_err(dev, "Failed to parse bypass value.\n");
		return ret;
	}
	spin_lock(&user_lock);
	if (val)
		drvdata->bypass = 1;
	else
		drvdata->bypass = 0;
	spin_unlock(&user_lock);
	return count;
}
static DEVICE_ATTR_RW(bypass);
#endif

static struct attribute *spienc_attr[] = {
	&dev_attr_status.attr,
#ifdef CONFIG_ARTINCHIP_SPIENC_DEBUG
	&dev_attr_bypass.attr,
#endif
	NULL
};

static int aic_spienc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct aic_spienc_drvdata *drvdata;
	struct reset_control *rst;
	int irq, ret;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata) {
		dev_err(dev, "Failed to malloc drvdata.\n");
		return -ENOMEM;
	}

	drvdata->dev = dev;
	drvdata->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(drvdata->base))
		return PTR_ERR(drvdata->base);

	writel(0x3F, (drvdata->base + SPIE_REG_ISR));

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "get irq failed.\n");
		return irq;
	}

	ret = devm_request_threaded_irq(dev, irq, aic_spienc_irq_handler,
					aic_spienc_irq_thread, IRQF_ONESHOT,
					dev_name(dev), drvdata);
	if (ret) {
		dev_err(dev, "Request IRQ failed.\n");
		return ret;
	}

	drvdata->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(drvdata->clk)) {
		dev_err(dev, "Could not get clock\n");
		return PTR_ERR(drvdata->clk);
	}

	drvdata->tweak = 0;
	of_property_read_u32(dev->of_node, "aic,spienc-tweak", &drvdata->tweak);

	ret = clk_prepare_enable(drvdata->clk);
	if (ret) {
		dev_err(drvdata->dev, "Failed to enable clock\n");
		return ret;
	}

	rst = devm_reset_control_get(dev, NULL);
	if (!IS_ERR(rst)) {
		reset_control_assert(rst);
		udelay(2);
		reset_control_deassert(rst);
	}

	platform_set_drvdata(pdev, drvdata);

	spienc_alg.drvdata = drvdata;
	ret = crypto_register_skcipher(&spienc_alg.alg);
	if (ret) {
		dev_err(dev, "Could not register algo for SPI Enc\n");
		goto err;
	}

	drvdata->attrs.attrs = spienc_attr;
	ret = sysfs_create_group(&pdev->dev.kobj, &drvdata->attrs);
	if (ret) {
		dev_err(dev, "sysfs create group failed.\n");
		goto err;
	}

	dev_info(dev, "SPI Enc Initialized\n");

	return 0;

err:
	clk_disable_unprepare(drvdata->clk);
	platform_set_drvdata(pdev, NULL);

	return ret;
}

static int aic_spienc_remove(struct platform_device *pdev)
{
	struct aic_spienc_drvdata *drvdata = platform_get_drvdata(pdev);

	if (!drvdata) {
		dev_err(&pdev->dev, "drvdata is not found.\n");
		return -ENODEV;
	}

	crypto_unregister_skcipher(&spienc_alg.alg);
	clk_disable_unprepare(drvdata->clk);
	kfree(drvdata);

	return 0;
}

static const struct of_device_id spienc_of_id_table[] = {
	{ .compatible = "artinchip,aic-spienc-v1.0" },
	{}
};
MODULE_DEVICE_TABLE(of, spienc_of_id_table);

static struct platform_driver spienc_driver = {
	.probe		= aic_spienc_probe,
	.remove		= aic_spienc_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.of_match_table	= spienc_of_id_table,
	},
};

static int __init aic_spienc_init(void)
{
	return platform_driver_register(&spienc_driver);
}
subsys_initcall(aic_spienc_init);

static void __exit aic_spienc_exit(void)
{
	return platform_driver_unregister(&spienc_driver);
}
module_exit(aic_spienc_exit);

MODULE_AUTHOR("Dehuang Wu <dehuang.wu@artinchip.com>");
MODULE_DESCRIPTION("Support for Artinchip SoC's SPI Enc");
MODULE_LICENSE("GPL");
