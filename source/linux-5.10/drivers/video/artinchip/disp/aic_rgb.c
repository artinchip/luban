// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors: matteo <duanmt@artinchip.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/component.h>
#include <linux/reset.h>
#include <linux/pinctrl/consumer.h>
#include <dt-bindings/display/artinchip,aic-disp.h>

#include "hw/rgb_reg.h"
#include "hw/reg_util.h"
#include "aic_fb.h"

struct aic_rgb_comp {
	/* di_funcs must be the first member */
	struct di_funcs funcs;
	struct device *dev;
	void __iomem *regs;
	struct reset_control *reset;
	struct clk *mclk;
	struct clk *sclk;
	struct panel_rgb *rgb;
	ulong sclk_rate;
};
static struct aic_rgb_comp *g_aic_rgb_comp;

/*TODO: static function begin*/

static struct aic_rgb_comp *aic_rgb_request_drvdata(void)
{
	return g_aic_rgb_comp;
}

static void aic_rgb_release_drvdata(void)
{

}

static inline int check_order_index(unsigned int data_order)
{
	u32 index[] = { 0x10, 0x01, 0x12, 0x21, 0x20, 0x02 };
	int mask, i;

	mask = data_order & 0xFF;

	for (i = 0; i < ARRAY_SIZE(index); i++) {
		if (index[i] == mask)
			return i;
	}
	return 0;
}

static ssize_t
info_show(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	char *order[] = { "RGB", "RBG", "BGR", "BRG", "GRB", "GBR" };
	struct panel_rgb *rgb = comp->rgb;
	int length;

	length = sprintf(buf, "RGB INFO\n"
			"\tmode\t\t : %d\n"
			"\tformat\t\t : %d\n"
			"\tclock_phase\t : %d\n"
			"\tdata_order\t : %s\n"
			"\tdata_mirror\t : %d\n",
			rgb->mode,
			rgb->format,
			rgb->clock_phase,
			order[check_order_index(rgb->data_order)],
			rgb->data_mirror);

	aic_rgb_release_drvdata();
	return length;
}

static ssize_t
format_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	rgb->format = val;

	if (rgb->mode == PRGB) {
		reg_set_bits(comp->regs + RGB_LCD_CTL,
				RGB_LCD_CTL_PRGB_MODE_MASK,
				RGB_LCD_CTL_PRGB_MODE(val));

		aic_rgb_release_drvdata();
		return count;
	}

	if (val)
		reg_set_bit(comp->regs + RGB_LCD_CTL, RGB_LCD_CTL_SRGB_MODE);
	else
		reg_clr_bit(comp->regs + RGB_LCD_CTL, RGB_LCD_CTL_SRGB_MODE);

	aic_rgb_release_drvdata();
	return count;
}

static ssize_t
clock_phase_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);
	if (err)
		return err;

	if (val > 3) {
		pr_err("Invalid clock phase, range [0, 3]\n");
		aic_rgb_release_drvdata();
		return count;
	}

	rgb->clock_phase = val;
	reg_set_bits(comp->regs + RGB_CLK_CTL,
		CKO_PHASE_SEL_MASK, CKO_PHASE_SEL(val));

	aic_rgb_release_drvdata();
	return count;
}

static ssize_t
data_mirror_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	rgb->data_mirror = enable;
	if (rgb->data_mirror)
		reg_set_bits(comp->regs + RGB_DATA_SEQ_SEL,
			RGB_DATA_OUT_SEL_MASK, RGB_DATA_OUT_SEL(7));
	else
		reg_clr_bits(comp->regs + RGB_DATA_SEQ_SEL,
			RGB_DATA_OUT_SEL_MASK);

	aic_rgb_release_drvdata();
	return count;
}

static ssize_t
data_order_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;
	char *order[] = { "RGB", "RBG", "BGR", "BRG", "GRB", "GBR" };
	u32 val[] = { 0x02100210, 0x02010201, 0x00120012,
		      0x00210021, 0x01200120, 0x01020102 };
	int i;

	for (i = 0; i < ARRAY_SIZE(order); i++)
		if (strncasecmp(buf, order[i], strlen(order[i])) == 0)
			break;

	if (i >= ARRAY_SIZE(order)) {
		pr_err("Invalid output order\n");
		return count;
	}

	rgb->data_order = val[i];
	reg_write(comp->regs + RGB_DATA_SEL, rgb->data_order);

	aic_rgb_release_drvdata();
	return count;
}

static DEVICE_ATTR_RO(info);
static DEVICE_ATTR_WO(format);
static DEVICE_ATTR_WO(clock_phase);
static DEVICE_ATTR_WO(data_mirror);
static DEVICE_ATTR_WO(data_order);

static struct attribute *aic_rgb_attrs[] = {
	&dev_attr_info.attr,
	&dev_attr_format.attr,
	&dev_attr_clock_phase.attr,
	&dev_attr_data_mirror.attr,
	&dev_attr_data_order.attr,
	NULL
};

static const struct attribute_group aic_rgb_attr_group = {
	.attrs = aic_rgb_attrs,
	.name  = "debug",
};

static void aic_rgb_swap(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;

	if (rgb->data_mirror)
		reg_set_bits(comp->regs + RGB_DATA_SEQ_SEL,
			RGB_DATA_OUT_SEL_MASK, RGB_DATA_OUT_SEL(7));

	if (rgb->data_order)
		reg_write(comp->regs + RGB_DATA_SEL, rgb->data_order);

	if (rgb->clock_phase)
		reg_set_bits(comp->regs + RGB_CLK_CTL,
			CKO_PHASE_SEL_MASK,
			CKO_PHASE_SEL(rgb->clock_phase));

	aic_rgb_release_drvdata();
}

static int aic_rgb_clk_enable(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	int ret = 0;

	if (comp->sclk_rate)
		clk_set_rate(comp->sclk, comp->sclk_rate);
	else
		dev_warn(comp->dev, "Use the default clock rate %ld\n",
			clk_get_rate(comp->sclk));

	ret = reset_control_deassert(comp->reset);
	if (ret) {
		dev_err(comp->dev, "Couldn't deassert\n");
		aic_rgb_release_drvdata();
		return ret;
	}

	ret = clk_prepare_enable(comp->mclk);
	if (ret) {
		dev_err(comp->dev, "Couldn't enable mclk\n");
		aic_rgb_release_drvdata();
		return ret;
	}

	aic_rgb_release_drvdata();
	return 0;
}

static int aic_rgb_clk_disable(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();

	clk_disable_unprepare(comp->mclk);
	reset_control_assert(comp->reset);
	aic_rgb_release_drvdata();
	return 0;
}

static int aic_rgb_enable(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;

	pinctrl_pm_select_default_state(comp->dev);
	reg_set_bits(comp->regs + RGB_LCD_CTL,
			RGB_LCD_CTL_MODE_MASK,
			RGB_LCD_CTL_MODE(rgb->mode));

	switch (rgb->mode) {
	case PRGB:
		reg_set_bits(comp->regs + RGB_LCD_CTL,
				RGB_LCD_CTL_PRGB_MODE_MASK,
				RGB_LCD_CTL_PRGB_MODE(rgb->format));
		break;
	case SRGB:
		if (rgb->format)
			reg_set_bit(comp->regs + RGB_LCD_CTL,
				RGB_LCD_CTL_SRGB_MODE);
		break;
	default:
		dev_err(comp->dev, "Invalid mode %d\n", rgb->mode);
		break;
	}

	aic_rgb_swap();
	reg_set_bit(comp->regs + RGB_LCD_CTL, RGB_LCD_CTL_EN);
	aic_rgb_release_drvdata();
	return 0;
}

static int aic_rgb_disable(void)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();

	reg_clr_bit(comp->regs + RGB_LCD_CTL, RGB_LCD_CTL_EN);
	aic_rgb_release_drvdata();
	pinctrl_pm_select_sleep_state(comp->dev);
	return 0;
}

static int aic_rgb_pixclk2mclk(ulong pixclk)
{
	s32 ret = 0;
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();
	struct panel_rgb *rgb = comp->rgb;

	dev_dbg(comp->dev, "Current pix-clk is %ld\n", pixclk);
	if (rgb->mode == PRGB)
		comp->sclk_rate = pixclk * 4;
	else if (rgb->mode == SRGB)
		comp->sclk_rate = pixclk * 12;

	aic_rgb_release_drvdata();
	return ret;
}

static int aic_rgb_attach_panel(struct aic_panel *panel)
{
	struct aic_rgb_comp *comp = aic_rgb_request_drvdata();

	comp->rgb = panel->rgb;
	return 0;
}

static int init_module_funcs(struct device *dev)
{
	struct aic_rgb_comp *comp = dev_get_drvdata(dev);

	comp->funcs.clk_enable = aic_rgb_clk_enable;
	comp->funcs.clk_disable = aic_rgb_clk_disable;
	comp->funcs.enable = aic_rgb_enable;
	comp->funcs.disable = aic_rgb_disable;
	comp->funcs.attach_panel = aic_rgb_attach_panel;
	comp->funcs.pixclk2mclk = aic_rgb_pixclk2mclk;
	return 0;
}

static int aic_rgb_bind(struct device *dev, struct device *master,
		     void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aic_rgb_comp *comp;
	struct resource *res;
	void __iomem *regs;
	int ret;

	comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
	if (!comp)
		return -ENOMEM;

	comp->dev = dev;
	dev_set_drvdata(dev, comp);
	g_aic_rgb_comp = comp;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	comp->regs = regs;

	comp->mclk = devm_clk_get(dev, "rgb0");
	if (IS_ERR(comp->mclk)) {
		dev_err(dev, "Couldn't get rgb0 clock\n");
		return PTR_ERR(comp->mclk);
	}

	comp->sclk = devm_clk_get(dev, "sclk");
	if (IS_ERR(comp->sclk)) {
		dev_err(dev, "Couldn't get sclk clock\n");
		return PTR_ERR(comp->sclk);
	}

	comp->reset = devm_reset_control_get(dev, "rgb0");
	if (IS_ERR(comp->reset)) {
		dev_err(dev, "Couldn't get reset line\n");
		return PTR_ERR(comp->reset);
	}

	ret = sysfs_create_group(&dev->kobj, &aic_rgb_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create sysfs node.\n");
		return ret;
	}

	init_module_funcs(dev);
	return 0;
}

static void aic_rgb_unbind(struct device *dev, struct device *master,
			void *data)
{
	sysfs_remove_group(&dev->kobj, &aic_rgb_attr_group);
}

static const struct component_ops aic_rgb_com_ops = {
	.bind	= aic_rgb_bind,
	.unbind	= aic_rgb_unbind,
};

static int aic_rgb_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s()\n", __func__);
	return component_add(&pdev->dev, &aic_rgb_com_ops);
}

static int aic_rgb_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &aic_rgb_com_ops);
	return 0;
}

static const struct of_device_id aic_rgb_match_table[] = {
	{.compatible = "artinchip,aic-rgb-v1.0"},
	{},
};

MODULE_DEVICE_TABLE(of, aic_rgb_match_table);

static struct platform_driver aic_rgb_driver = {
	.probe = aic_rgb_probe,
	.remove = aic_rgb_remove,
	.driver = {
		.name = "disp rgb",
		.of_match_table	= aic_rgb_match_table,
	},
};

module_platform_driver(aic_rgb_driver);

MODULE_AUTHOR("matteo<duanmt@artinchip.com>");
MODULE_DESCRIPTION("AIC disp RGB driver");
MODULE_ALIAS("platform:rgb");
MODULE_LICENSE("GPL v2");
