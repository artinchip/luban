// SPDX-License-Identifier: GPL-2.0-only
/*
 * Warm Reset Info driver of ArtInChip SoC
 *
 * Copyright (C) 2020-2025 ArtInChip Technology Co., Ltd.
 * Authors:  Siyao.Li <siyao.li@artinchip.com>
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/reboot-reason.h>
#include <linux/nvmem-provider.h>

#define AIC_WRI_NAME			"aic-wri"

/* Register definition for RTC */
#define RTC_REBOOT_REASON_MASK		GENMASK(7, 4)
#define RTC_REBOOT_REASON_SHIFT		4
#define RTC_WR_EN_KEY			0xAC
#define RTC_REG_SYSBAK			0x100
#define RTC_REG_SYSBAK_OFFSET		0x4
#define WRI_RTC_WRITE_ENABLE(base)	writeb(RTC_WR_EN_KEY, base)
#define WRI_RTC_WRITE_DISABLE(base)	writeb(0, base)
#define WRI_RTC_WRITEB(val, reg, base) \
	do { \
		WRI_RTC_WRITE_ENABLE(base); \
		writeb((val) & 0xFF, (base) + (reg)); \
		WRI_RTC_WRITE_DISABLE(base); \
	} while (0)

/* Register definition for WRI */
#define WRI_RST_FLAG			0x000
#define WRI_VER				0xFFC

#define WRI_FLAG_CMP			BIT(12)
#define WRI_FLAG_OTP			BIT(11)
#define WRI_FLAG_WDOG_RST		BIT(10)
#define WRI_FLAG_DM_RST			BIT(9)
#define WRI_FLAG_EXT_RST		BIT(8)
#define WRI_FLAG_RTC_POR		BIT(1)
#define WRI_FLAG_SYS_POR		BIT(0)

struct aic_wri_dev {
	struct attribute_group attrs;
	struct platform_device *pdev;
	struct device *dev;
	void __iomem *regs;
};

enum aic_warm_reset_type {
	WRI_TYPE_POR = 0,
	WRI_TYPE_RTC,
	WRI_TYPE_EXT,
	WRI_TYPE_DM,
	WRI_TYPE_WDOG,
	WRI_TYPE_OTP,
	WRI_TYPE_CMP,

	WRI_TYPE_MAX
};

struct reboot_info {
	u32 inited;
	enum aic_reboot_reason sw_reason;
	enum aic_warm_reset_type hw_reason;
};

static struct reboot_info g_last_reboot = {0};
static void __iomem *g_rtc_wr_base;
static enum aic_reboot_reason g_prev_reason = REBOOT_REASON_INVALID;

static char *reason[] = {"Cold Reboot", "CMD Reboot", "CMD Shutdown",
			 "Suspend", "Upgrade", "U-Boot Upgrade", "", "",
			 "SW Lockup", "Hw Lockup", "Panic", "Ramdump",
			 "", "", "", ""};

static DEFINE_SPINLOCK(user_lock);

void aic_set_software_reboot_reason(enum aic_reboot_reason r)
{
	u32 cur = 0;

	if (!g_rtc_wr_base)
		return;

	spin_lock(&user_lock);
	cur = readb(g_rtc_wr_base + RTC_REG_SYSBAK_OFFSET);

	if (cur >> RTC_REBOOT_REASON_SHIFT) {
		spin_unlock(&user_lock);
		return;
	}

	cur |= (r << RTC_REBOOT_REASON_SHIFT) & RTC_REBOOT_REASON_MASK;
	WRI_RTC_WRITEB(cur, RTC_REG_SYSBAK_OFFSET, g_rtc_wr_base);
	spin_unlock(&user_lock);

	if (likely(r <= RTC_REBOOT_REASON_MASK >> RTC_REBOOT_REASON_SHIFT))
		pr_info("Set reboot reason: [%d] %s\n", r, reason[r]);
}

enum aic_reboot_reason aic_get_software_reboot_reason(void)
{
	u32 cur = 0;

	if (!g_rtc_wr_base)
		return REBOOT_REASON_INVALID;

	if (g_prev_reason == REBOOT_REASON_INVALID) {
		spin_lock(&user_lock);
		cur = readb(g_rtc_wr_base + RTC_REG_SYSBAK_OFFSET);
		g_prev_reason = cur >> RTC_REBOOT_REASON_SHIFT;
		cur &= ~RTC_REBOOT_REASON_MASK;

		WRI_RTC_WRITEB(cur, RTC_REG_SYSBAK_OFFSET, g_rtc_wr_base);
		spin_unlock(&user_lock);
	}

	pr_info("The software reboot reason: [%d] %s\n", g_prev_reason, reason[g_prev_reason]);
	return g_prev_reason;
}

void aic_set_reboot_reason(enum aic_reboot_reason r)
{
	aic_set_software_reboot_reason(r);
}
EXPORT_SYMBOL_GPL(aic_set_reboot_reason);

static int aic_rtc_nvmem_write(void *priv, unsigned int offset, void *val,
			       size_t bytes)
{
	int i;
	u32 *pval = (u32 *)val;

	pr_debug("%s() write %ld byte, offset %d\n", __func__, bytes, offset);
	spin_lock(&user_lock);
	for (i = 0; i < (bytes + 3) / 4; i++, pval++) {
		WRI_RTC_WRITEB(*pval, RTC_REG_SYSBAK_OFFSET + offset + i * 4, g_rtc_wr_base);
		pr_debug("%s() SYS_BAK reg %#x: %#x\n", __func__,
			 RTC_REG_SYSBAK + offset + i * 4, *pval);
	}
	spin_unlock(&user_lock);

	return 0;
}

static int aic_rtc_nvmem_read(void *priv, unsigned int offset,
			      void *val, size_t bytes)
{
	int i;
	u32 *pval = (u32 *)val;

	pr_debug("%s() read %ld byte, offset %d\n", __func__, bytes, offset);
	spin_lock(&user_lock);
	for (i = 0; i < (bytes + 3) / 4; i++, pval++) {
		*pval = readb(g_rtc_wr_base + RTC_REG_SYSBAK_OFFSET + offset + i * 4);
		pr_debug("%s() SYS_BAK reg %#x: %#x\n", __func__,
			 RTC_REG_SYSBAK + offset + i * 4, *pval);
	}
	spin_unlock(&user_lock);

	return 0;
}

enum aic_warm_reset_type aic_hw_type_get(void __iomem *regs)
{
	u32 val = 0;
	s8 i;
	u16 wr_bit[WRI_TYPE_MAX] = {WRI_FLAG_SYS_POR, WRI_FLAG_RTC_POR,
				    WRI_FLAG_EXT_RST, WRI_FLAG_DM_RST,
				    WRI_FLAG_WDOG_RST, WRI_FLAG_OTP,
				    WRI_FLAG_CMP};

	val = readl(regs + WRI_RST_FLAG);
	if (!val)
		return WRI_TYPE_POR;

	writel(val, regs + WRI_RST_FLAG);
	for (i = WRI_TYPE_MAX - 1; i >= 0; i--) {
		if (val & wr_bit[i]) {
			g_last_reboot.hw_reason = (enum aic_warm_reset_type)i;
			return g_last_reboot.hw_reason;
		}
	}

	pr_warn("Invalid warm reset flag: %#x\n", val);
	return WRI_TYPE_POR;
}

enum aic_reboot_reason aic_sw_type_get(void)
{
	g_last_reboot.sw_reason = aic_get_software_reboot_reason();
	return g_last_reboot.sw_reason;
}

enum aic_reboot_reason aic_judge_reboot_reason(enum aic_warm_reset_type hw,
					       enum aic_reboot_reason sw)
{
	enum aic_reboot_reason r = (enum aic_reboot_reason)sw;

	/* First, check the software-triggered reboot */
	if (hw == WRI_TYPE_WDOG) {
		pr_info("Reboot action: Watchdog-Reset");
		switch (sw) {
		case REBOOT_REASON_UPGRADE:
			pr_info("Reboot reason: Upgrade-Mode\n");
			break;
		case REBOOT_REASON_CMD_REBOOT:
			pr_info("Reboot reason: Command-Reboot\n");
			break;
		case REBOOT_REASON_SW_LOCKUP:
			pr_info("Reboot reason: Software-Lockup\n");
			break;
		case REBOOT_REASON_HW_LOCKUP:
			pr_info("Reboot reason: Hardware-Lockup\n");
			break;
		case REBOOT_REASON_PANIC:
			pr_info("Reboot reason: Kernel-Panic\n");
			break;
		case REBOOT_REASON_RAMDUMP:
			pr_info("Ramdump\n");
			break;
		default:
			pr_info("Unknown(%d)\n", r);
			break;
		}
		return r;
	}

	if (r == REBOOT_REASON_CMD_SHUTDOWN) {
		pr_info("Reboot reason: Command-Poweroff\n");
		return r;
		}

	if (r == REBOOT_REASON_SUSPEND) {
		pr_info("Reboot reason: Suspend\n");
		return r;
	}

	/* Second, check the hardware-triggered reboot */
	if (r == REBOOT_REASON_COLD) {
		if (hw == WRI_TYPE_POR) {
			pr_info("Startup reason: Power-On-Reset\n");
			return (enum aic_reboot_reason)sw;
		}

		pr_info("Reboot action: Warm-Reset");
		switch (hw) {
		case WRI_TYPE_RTC:
			pr_info("Reboot reason: RTC-Power-Down\n");
			r = REBOOT_REASON_RTC;
			break;
		case WRI_TYPE_EXT:
			pr_info("Reboot reason: Extend-Reset\n");
			r = REBOOT_REASON_EXTEND;
			break;
		case WRI_TYPE_DM:
			pr_info("Reboot reason: JTAG-Reset\n");
			r = REBOOT_REASON_DM;
			break;
		case WRI_TYPE_OTP:
			pr_info("Reboot reason: OTP-Reset\n");
			r = REBOOT_REASON_OTP;
			break;
		case WRI_TYPE_CMP:
			pr_info("Reboot reason: Undervoltage-Reset\n");
			r = REBOOT_REASON_UNDER_VOL;
			break;
		default:
			pr_info("Unknown(%d)\n", hw);
			break;
		}
		return r;
	}

	pr_warn("Unknown reboot reason: %d - %d\n", hw, sw);
	return r;
}

static ssize_t reboot_reason_show(struct device *dev,
				  struct device_attribute *devattr,
				  char *buf)
{
	struct aic_wri_dev *wri = dev_get_drvdata(dev);
	void __iomem *regs = wri->regs;
	enum aic_warm_reset_type hw;
	enum aic_reboot_reason sw;

	if (g_last_reboot.inited)
		return sprintf(buf, "%d\n",
			       aic_judge_reboot_reason(g_last_reboot.hw_reason,
						       g_last_reboot.sw_reason)
						       );
	hw = aic_hw_type_get(regs);
	sw = aic_sw_type_get();
	g_last_reboot.inited = 1;
	return sprintf(buf, "%d\n", aic_judge_reboot_reason(hw, sw));
}
static DEVICE_ATTR_RO(reboot_reason);

static ssize_t soft_reboot_reason_show(struct device *dev,
				  struct device_attribute *devattr,
				  char *buf)
{
	return sprintf(buf, "%d\n", aic_get_software_reboot_reason());
}
static DEVICE_ATTR_RO(soft_reboot_reason);

static ssize_t aicupg_store(struct device *dev,
			    struct device_attribute *devattr,
			    const char *buf, size_t count)
{
	if (strlen(buf) == 2 && !strncmp(buf, "1", 1)) {
		/* Goto BROM upgmode */
		aic_set_software_reboot_reason(REBOOT_REASON_UPGRADE);
	} else if (strlen(buf) == 2 && !strncmp(buf, "2", 1)) {
		/* Goto U-Boot upgmode */
		aic_set_software_reboot_reason(REBOOT_REASON_BL_UPGRADE);
	} else {
		dev_info(dev, "Invalid argument: %s\n", buf);
	}
	return count;
}
static DEVICE_ATTR_WO(aicupg);

static struct attribute *aic_wri_attr[] = {
	&dev_attr_reboot_reason.attr,
	&dev_attr_soft_reboot_reason.attr,
	&dev_attr_aicupg.attr,
	NULL
};

static int aic_wri_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nvmem_config *nvmem_cfg;
	struct nvmem_device *nvmem;
	struct aic_wri_dev *wri;
	int ret = 0;

	wri = devm_kzalloc(&pdev->dev, sizeof(struct aic_wri_dev),
			   GFP_KERNEL);
	if (!wri)
		return -ENOMEM;

	wri->pdev = pdev;

	wri->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wri->regs))
		return PTR_ERR(wri->regs);

	g_rtc_wr_base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(g_rtc_wr_base))
		return PTR_ERR(g_rtc_wr_base);

	wri->attrs.attrs = aic_wri_attr;
	ret = sysfs_create_group(&pdev->dev.kobj, &wri->attrs);
	if (ret)
		return ret;

	nvmem_cfg = devm_kzalloc(dev, sizeof(*nvmem_cfg), GFP_KERNEL);
	if (!nvmem_cfg)
		return -ENOMEM;

	nvmem_cfg->dev = dev;
	nvmem_cfg->name = "rtc_nvram";
	nvmem_cfg->size = 64;
	nvmem_cfg->word_size = 4;
	nvmem_cfg->stride = 4;
	nvmem_cfg->priv = wri;
	nvmem_cfg->reg_read = aic_rtc_nvmem_read;
	nvmem_cfg->reg_write = aic_rtc_nvmem_write;
	nvmem = devm_nvmem_register(dev, nvmem_cfg);
	if (IS_ERR(nvmem))
		return PTR_ERR(nvmem);

	platform_set_drvdata(pdev, nvmem);

	aic_get_software_reboot_reason();
	platform_set_drvdata(pdev, wri);
	dev_info(&pdev->dev, "ArtInChip WRI Loaded\n");
	return PTR_ERR_OR_ZERO(nvmem);
}

static const struct of_device_id aic_wri_of_match[] = {
	{
		.compatible = "artinchip,aic-wri-v1.0",
	},
	{}
};
MODULE_DEVICE_TABLE(of, aic_wri_of_match);

static struct platform_driver aic_wri_driver = {
	.driver = {
		.name = AIC_WRI_NAME,
		.of_match_table = of_match_ptr(aic_wri_of_match),
	},
	.probe = aic_wri_probe,
};
module_platform_driver(aic_wri_driver);

MODULE_AUTHOR("Siyao Li <siyao.li@artinchip.com>");
MODULE_DESCRIPTION("Warm Reset Info driver of ArtInChip SoC");
MODULE_LICENSE("GPL");
