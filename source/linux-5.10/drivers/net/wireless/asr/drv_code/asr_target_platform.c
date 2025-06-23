/**
 ******************************************************************************
 *
 * @file asr_target_platform.c
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/ktime.h>
#include "asr_defs.h"
#include "asr_sdio.h"
#include "asr_hif.h"
#include "asr_utils.h"
#include <linux/delay.h>	/* mdelay() */
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_PINCTRL
#include <linux/pinctrl/consumer.h>
#endif
#include <linux/clk.h>
#ifdef ASR_MACH_PXA1826_CLK_EN
#include <linux/platform_data/pxa_sdhci.h>
#endif
#include "asr_platform.h"

#ifdef CONFIG_NOT_USED_DTS

#ifdef ASR_MODULE_POWER_PIN_SUPPORT
#define ASR_PLAT_POWER_PIN		0	//GPIO68
#endif

#ifdef ASR_MODULE_RESET_SUPPORT
#define ASR_PLAT_RESET_PIN		0	//GPIO67
#endif

#endif

//=======================sdio func start========================================
#ifdef CONFIG_ASR_SDIO

extern void asr_sdio_detect_change(void);

//=======================pinctrl func start========================================
#ifndef CONFIG_NOT_USED_DTS
#ifdef CONFIG_PINCTRL
static inline void asr_get_pinctrl(struct device *dev)
{
	if (!g_asr_para.asr_pinctrl || IS_ERR(g_asr_para.asr_pinctrl)) {
		g_asr_para.asr_pinctrl = devm_pinctrl_get(dev);
	}
}

static inline void asr_put_pinctrl(struct device *dev)
{
	if (g_asr_para.asr_pinctrl && !IS_ERR(g_asr_para.asr_pinctrl)) {
		devm_pinctrl_put(g_asr_para.asr_pinctrl);
	}
	g_asr_para.asr_pinctrl = NULL;
}
#endif
#endif
//=======================pinctrl func end========================================

//=======================boot pin to rts func start========================================
#ifdef ASR_BOOT_TO_RTS_PIN_SUPPORT
static int asr_set_rts_as_gpio(struct device *dev)
{
	int ret = 0;

#ifdef CONFIG_PINCTRL
	if (!g_asr_para.boot_pins_gpio || IS_ERR(g_asr_para.boot_pins_gpio)) {
		dev_err(dev, "%s: failed to get boot_pins_gpio pinctrl state\n", __func__);
		return -EINVAL;
	}

	ret = pinctrl_select_state(g_asr_para.asr_pinctrl, g_asr_para.boot_pins_gpio);
	if (ret) {
		dev_err(dev, "%s: failed to select boot_pins_gpio pinctrl state\n", __func__);
		return -EINVAL;
	}
	//devm_pinctrl_put(g_asr_para.asr_pinctrl);
	dev_info(dev, "%s: success\n", __func__);
#endif
	return 0;
}

static int asr_set_rts_as_uart(struct device *dev)
{
	int ret;

#ifdef CONFIG_PINCTRL
	if (!g_asr_para.boot_pins_uart || IS_ERR(g_asr_para.boot_pins_uart)) {
		dev_err(dev, "%s: failed to get boot_pins_uart pinctrl state\n", __func__);
		return -EINVAL;
	}

	ret = pinctrl_select_state(g_asr_para.asr_pinctrl, g_asr_para.boot_pins_uart);
	if (ret) {
		dev_err(dev, "%s: failed to select boot_pins_uart pinctrl state\n", __func__);
		return -EINVAL;
	}

	dev_info(dev, "%s: success\n", __func__);
#endif
	return 0;
}
#endif
//=======================boot pin to rts func end========================================

//=======================reset func start========================================
#ifdef ASR_MODULE_RESET_SUPPORT
static int asr_set_reset_as_gpio_on(struct device *dev, bool pins_on)
{
#ifndef CONFIG_NOT_USED_DTS
#ifdef CONFIG_PINCTRL
	int ret;

	if (!g_asr_para.reset_pins_on || !g_asr_para.reset_pins_off || IS_ERR(g_asr_para.reset_pins_on)
	    || IS_ERR(g_asr_para.reset_pins_off)) {
		dev_err(dev, "%s: failed to get reset pinctrl state\n", __func__);
		return -EINVAL;
	}

	if (pins_on) {
		ret = pinctrl_select_state(g_asr_para.asr_pinctrl, g_asr_para.reset_pins_on);
		if (ret) {
			dev_err(dev, "%s: failed to select reset pinctrl %d state\n", __func__, pins_on);
			return -EINVAL;
		}
	} else {
		ret = pinctrl_select_state(g_asr_para.asr_pinctrl, g_asr_para.reset_pins_off);
		if (ret) {
			dev_err(dev, "%s: failed to select reset pinctrl %d state\n", __func__, pins_on);
			return -EINVAL;
		}

	}

	dev_info(dev, "%s: %d success\n", __func__, pins_on);
	msleep(10);
#endif
#endif
	return 0;
}

int asr_set_wifi_reset(struct device *dev, u32 delay_ms)
{
	int ret = -1;

	if (g_asr_para.reset_pin < 0) {
		dev_err(dev, "%s: reset_pin %d invalid\n", __func__, g_asr_para.reset_pin);
		return -1;
	}
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	ret = devm_gpio_request(dev, g_asr_para.reset_pin, NULL);
	if (ret) {
		dev_err(dev, "%s: gpio_reset request %d fail: err = 0x%d\n", __func__, g_asr_para.reset_pin, ret);

		return ret;
	}
#else
	ret = gpio_request(g_asr_para.reset_pin, NULL);
	if (ret) {
		dev_err(dev, "%s: gpio_reset request %d fail: err = 0x%d\n", __func__, g_asr_para.reset_pin, ret);

		return ret;
	}
#endif

#ifdef ASR_BOOT_TO_RTS_PIN_SUPPORT
	//wifi module boot pin
	asr_set_rts_as_gpio(dev);
#endif

	gpio_direction_output(g_asr_para.reset_pin, 0);
	if (delay_ms) {
		msleep(delay_ms);
	}
	gpio_direction_output(g_asr_para.reset_pin, 1);
	if (delay_ms) {
		msleep(delay_ms);
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	devm_gpio_free(dev, g_asr_para.reset_pin);
#else
	gpio_free(g_asr_para.reset_pin);
#endif

#ifdef ASR_BOOT_TO_RTS_PIN_SUPPORT
	asr_set_rts_as_uart(dev);
#endif

	dev_err(dev, "%s: gpio_reset %d success\n", __func__, g_asr_para.reset_pin);

	return 0;
}
#endif
//=======================reset func end========================================

//=======================sdio sdhci start========================================
#ifdef ASR_SDIO_HOST_SDHCI
static int asr_get_sdio_host_sdhci(struct device *dev)
{
	struct device_node *np = dev->of_node, *sdh_np = NULL;
	struct platform_device *sdh_pdev = NULL;
	struct sdhci_host *host = NULL;
	int sdh_phandle = 0;

	if (of_property_read_u32(np, "sd-host", &sdh_phandle)) {
		dev_err(dev, "failed to find sd-host in dt\n");
		return -1;
	}

	/* we've got the phandle for sdh */
	sdh_np = of_find_node_by_phandle(sdh_phandle);
	if (unlikely(IS_ERR(sdh_np))) {
		dev_err(dev, "failed to find device_node for sdh\n");
		return -1;
	}

	sdh_pdev = of_find_device_by_node(sdh_np);
	if (unlikely(IS_ERR(sdh_pdev))) {
		dev_err(dev, "failed to find platform_device for sdh\n");
		return -1;
	}

	/* sdh_pdev->dev->driver_data was set as sdhci_host in sdhci driver */
	host = platform_get_drvdata(sdh_pdev);

	/*
	 * If we cannot find host, it's because sdh device is not registered
	 * yet. Probe again later.
	 */
	if (!host) {
		dev_err(dev, "failed to find sdio host\n");
		return -EPROBE_DEFER;
	}

	g_asr_para.mmc = host->mmc;

	dev_err(dev, "%s: get sdio host(%s) success\n", __func__, mmc_hostname(g_asr_para.mmc));

	return 0;
}
#endif
//=======================sdio sdhci end========================================

static int asr_get_sdio_host(struct device *dev)
{
#ifdef ASR_SDIO_HOST_SDHCI
	return asr_get_sdio_host_sdhci(dev);
#else
	return 0;
#endif
}

//=======================platform no dts start========================================
#ifndef CONFIG_NOT_USED_DTS
static int asr_get_platfrom_info_dts(struct device *dev)
{

#ifdef ASR_MODULE_RESET_SUPPORT
#ifdef CONFIG_PINCTRL
	if (!g_asr_para.reset_pins_on || IS_ERR(g_asr_para.reset_pins_on)) {
		g_asr_para.reset_pins_on = pinctrl_lookup_state(g_asr_para.asr_pinctrl, "reset_on");
	}

	if (!g_asr_para.reset_pins_off || IS_ERR(g_asr_para.reset_pins_off)) {
		g_asr_para.reset_pins_off = pinctrl_lookup_state(g_asr_para.asr_pinctrl, "reset_off");
	}

	if (IS_ERR(g_asr_para.reset_pins_on)
	    || IS_ERR(g_asr_para.reset_pins_off)) {
		dev_err(dev, "%s: failed to get reset pinctrl state\n", __func__);
		return -EINVAL;
	}
#endif

#ifdef CONFIG_OF
	if (g_asr_para.reset_pin < 0) {
		g_asr_para.reset_pin = of_get_named_gpio(dev->of_node, "asr_fw,reset-gpios", 0);
	}

	if (!gpio_is_valid(g_asr_para.reset_pin)) {
		dev_err(dev, "%s:of_get_named_gpio reset %d faild\n", __func__, g_asr_para.reset_pin);
		return -1;
	} else {
		dev_err(dev, "%s:of_get_named_gpio reset %d success\n", __func__, g_asr_para.reset_pin);
	}
#endif
#endif

#ifdef ASR_MODULE_POWER_PIN_SUPPORT
#ifdef CONFIG_PINCTRL
	if (!g_asr_para.power_pins_on || IS_ERR(g_asr_para.power_pins_on)) {
		g_asr_para.power_pins_on = pinctrl_lookup_state(g_asr_para.asr_pinctrl, "power_on");
	}

	if (!g_asr_para.power_pins_off || IS_ERR(g_asr_para.power_pins_off)) {
		g_asr_para.power_pins_off = pinctrl_lookup_state(g_asr_para.asr_pinctrl, "power_off");
	}

	if (IS_ERR(g_asr_para.power_pins_on)
	    || IS_ERR(g_asr_para.power_pins_off)) {
		dev_err(dev, "%s: failed to get power pinctrl state\n", __func__);
		return -EINVAL;
	}
#endif
#endif

#ifdef ASR_BOOT_TO_RTS_PIN_SUPPORT
#ifdef CONFIG_PINCTRL
	if (!g_asr_para.boot_pins_uart || IS_ERR(g_asr_para.boot_pins_uart)) {
		g_asr_para.boot_pins_uart = pinctrl_lookup_state(g_asr_para.asr_pinctrl, "boot_pin_uart");
	}

	if (!g_asr_para.boot_pins_gpio || IS_ERR(g_asr_para.boot_pins_gpio)) {
		g_asr_para.boot_pins_gpio = pinctrl_lookup_state(g_asr_para.asr_pinctrl, "boot_pins_gpio");
	}
#endif
#endif

#ifdef OOB_INTR_ONLY
#ifdef CONFIG_OF
		if (g_asr_para.oob_intr_pin < 0) {
			g_asr_para.oob_intr_pin = of_get_named_gpio(dev->of_node, "asr_fw,oob-gpios", 0);
		}

		if (!gpio_is_valid(g_asr_para.oob_intr_pin)) {
			dev_err(dev, "%s:of_get_named_gpio oob_intr_pin %d faild\n", __func__, g_asr_para.oob_intr_pin);
			return -1;
		} else {
			dev_err(dev, "%s:of_get_named_gpio oob_intr_pin %d success\n", __func__, g_asr_para.oob_intr_pin);
		}
#endif
#endif

	return 0;
}
#endif
//=======================platform no dts end========================================

static int asr_get_platfrom_info(struct device *dev)
{
#ifdef CONFIG_NOT_USED_DTS
	return 0;
#else
	return asr_get_platfrom_info_dts(dev);
#endif
}

//=======================module power ldo start========================================
#ifdef ASR_FINCH_MODULE_POWER_SUPPORT
static int asr_module_power_ldo_on(struct device *dev, bool power_on)
{
	int ret = 0;
	struct regulator *wifi_regulator = NULL;	/* wifi_3v3 power on */

#ifdef ASR_BOOT_TO_RTS_PIN_SUPPORT
	/* select 5825 wifi mode to sdio */
	asr_set_rts_as_gpio(dev);
	mdelay(100);
#endif

	/* power on */
	wifi_regulator = regulator_get(NULL, "LDO4");
	if (IS_ERR(wifi_regulator)) {
		dev_err(dev, "get wifi regulator failed!\n");
		return -1;
	}

	if (power_on) {
		regulator_set_voltage(wifi_regulator, 3300000, 3300000);
		ret = regulator_enable(wifi_regulator);
		if (ret) {
			dev_err(dev, "enable wifi voltage fail!\n");
			return ret;
		}
	} else {

		ret = regulator_disable(wifi_regulator);
		if (ret) {
			dev_err(dev, "disable wifi regulator fail!\n");
		}
	}

	regulator_put(wifi_regulator);
	mdelay(100);

	dev_info(dev, "%s: LDO power %d success\n", __func__, power_on);

	return 0;
}
#endif
//=======================module power ldo end========================================

//=======================module power pin start========================================
#ifdef ASR_MODULE_POWER_PIN_SUPPORT
#ifndef CONFIG_NOT_USED_DTS
static int asr_module_power_pin_on(struct device *dev, bool power_on)
{
#ifdef CONFIG_PINCTRL
	int ret = 0;

	if (!g_asr_para.power_pins_on || !g_asr_para.power_pins_off || IS_ERR(g_asr_para.power_pins_on)
	    || IS_ERR(g_asr_para.power_pins_off)) {
		dev_err(dev, "%s: failed to get power pinctrl state\n", __func__);
		return -EINVAL;
	}

	if (power_on) {
		ret = pinctrl_select_state(g_asr_para.asr_pinctrl, g_asr_para.power_pins_on);
		if (ret) {
			dev_err(dev, "%s: failed to select power pinctrl %d state\n", __func__, power_on);
			return -EINVAL;
		}
	} else {
		ret = pinctrl_select_state(g_asr_para.asr_pinctrl, g_asr_para.power_pins_off);
		if (ret) {
			dev_err(dev, "%s: failed to select power pinctrl %d state\n", __func__, power_on);
			return -EINVAL;
		}

	}

	dev_info(dev, "%s: pin power %d success\n", __func__, power_on);
	mdelay(50);
#endif

	return 0;
}
#else
static int asr_module_power_pin_on(struct device *dev, bool power_on)
{
	int ret = -1;

	if (g_asr_para.power_pin < 0) {
		dev_err(dev, "%s: power_pin %d invalid\n", __func__, g_asr_para.power_pin);
		return -1;
	}
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	ret = devm_gpio_request(dev, g_asr_para.power_pin, NULL);
	if (ret) {
		dev_err(dev, "%s: power_pin request %d fail: err = 0x%d\n", __func__, g_asr_para.power_pin, ret);

		return ret;
	}
#else
	ret = gpio_request(g_asr_para.power_pin, NULL);
	if (ret) {
		dev_err(dev, "%s: power_pin request %d fail: err = 0x%d\n", __func__, g_asr_para.power_pin, ret);

		return ret;
	}
#endif

	gpio_direction_output(g_asr_para.power_pin, power_on ? 1 : 0);

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
	devm_gpio_free(dev, g_asr_para.power_pin);
#else
	gpio_free(g_asr_para.power_pin);
#endif

	dev_err(dev, "%s: set power_pin(%d) success\n", __func__, g_asr_para.power_pin);

	return 0;
}
#endif
#endif
//=======================module power pin end========================================

static int asr_module_power_on(struct device *dev, bool power_on)
{
#ifdef ASR_FINCH_MODULE_POWER_SUPPORT
	return asr_module_power_ldo_on(dev, power_on);
#elif defined(ASR_MODULE_POWER_PIN_SUPPORT)
	return asr_module_power_pin_on(dev, power_on);
#else
	return 0;
#endif
}

static void asr_sdio_host_clk_en(bool clk_en)
{
#ifdef ASR_MACH_PXA1826_CLK_EN
	struct platform_device *pdev;
	struct sdhci_pxa_platdata *sdhci_pdata;

	if (g_asr_para.asr_hw && g_asr_para.asr_hw->plat && g_asr_para.asr_hw->plat->func
	    && g_asr_para.asr_hw->plat->func->card && g_asr_para.asr_hw->plat->func->card->host) {
		pdev = to_platform_device(mmc_dev(g_asr_para.asr_hw->plat->func->card->host));
		dev_info(g_asr_para.dev, "%s: use func host.\n", __func__);
	} else if (g_asr_para.mmc) {
		pdev = to_platform_device(mmc_dev(g_asr_para.mmc));
		dev_info(g_asr_para.dev, "%s: use dts host.\n", __func__);
	} else {
		dev_err(g_asr_para.dev, "%s: find mmc fail.\n", __func__);
		return;
	}

	sdhci_pdata = pdev->dev.platform_data;

	if (clk_en) {
		//fix clock issue for sdio rx
		clk_prepare_enable(sdhci_pdata->clk);
	} else {
		clk_disable_unprepare(sdhci_pdata->clk);
	}
	dev_err(g_asr_para.dev, "%s:clk=%p clk_en=%d\n", __func__, sdhci_pdata->clk, clk_en);
#endif
}


void asr_sdio_detect_change(void)
{
	struct mmc_host *mmc = NULL;

	if (g_asr_para.asr_hw && g_asr_para.asr_hw->plat && g_asr_para.asr_hw->plat->func
	    && g_asr_para.asr_hw->plat->func->card && g_asr_para.asr_hw->plat->func->card->host) {

		mmc = g_asr_para.asr_hw->plat->func->card->host;
		dev_info(g_asr_para.dev, "%s: use func host.\n", __func__);
	} else if (g_asr_para.mmc) {
		mmc = g_asr_para.mmc;
		dev_info(g_asr_para.dev, "%s: use dts host.\n", __func__);
	} else {
		dev_err(g_asr_para.dev, "%s: find mmc fail.\n", __func__);
		return;
	}

	dev_err(g_asr_para.dev, "%s: sdio host(%s)\n", __func__, mmc_hostname(mmc));

	mmc_detect_change(mmc, 0);

	return;
}

static int asr_sdio_power_on(struct device *dev)
{
	int ret = 0;

#ifdef ASR_MODULE_RESET_SUPPORT
#ifdef CONFIG_PINCTRL
	g_asr_para.reset_pins_on = NULL;
	g_asr_para.reset_pins_off = NULL;
#endif
#ifndef CONFIG_NOT_USED_DTS
	g_asr_para.reset_pin = -1;
#else
	g_asr_para.reset_pin = ASR_PLAT_RESET_PIN;
#endif
#endif

#ifdef ASR_MODULE_POWER_PIN_SUPPORT
#ifdef CONFIG_PINCTRL
	g_asr_para.power_pins_on = NULL;
	g_asr_para.power_pins_off = NULL;
#endif
#ifndef CONFIG_NOT_USED_DTS
	g_asr_para.power_pin = -1;
#else
	g_asr_para.power_pin = ASR_PLAT_POWER_PIN;
#endif
#endif

#ifdef OOB_INTR_ONLY
#ifndef CONFIG_NOT_USED_DTS
    g_asr_para.oob_intr_pin = -1;
#else
    g_asr_para.oob_intr_pin = 46;   // gpio1 14.
#endif
#endif

	ret = asr_get_sdio_host(dev);

#ifndef CONFIG_NOT_USED_DTS
#ifdef CONFIG_PINCTRL
	asr_get_pinctrl(dev);
#endif
#endif

	ret = asr_get_platfrom_info(dev);
	if (ret) {
		return ret;
	}

	ret = asr_module_power_on(dev, true);
	if (ret) {
		return ret;
	}
#ifdef ASR_MODULE_RESET_SUPPORT
	asr_set_reset_as_gpio_on(dev, true);
	asr_set_wifi_reset(dev, 50);
#endif

#ifdef ASR_BOOT_TO_RTS_PIN_SUPPORT
	asr_set_rts_as_uart(dev);
#endif

	/* do sdio detect */
	asr_sdio_host_clk_en(true);
	asr_sdio_detect_change();

	return 0;
}

static int asr_sdio_power_off(struct device *dev)
{

	if (asr_module_power_on(dev, false)) {
		return -1;
	}
#ifdef ASR_MODULE_RESET_SUPPORT
	asr_set_reset_as_gpio_on(dev, false);
	asr_set_wifi_reset(dev, 50);
	#ifdef CONFIG_PINCTRL
	g_asr_para.reset_pins_on = NULL;
	g_asr_para.reset_pins_off = NULL;
	#endif
	g_asr_para.reset_pin = -1;
#endif

	/* do sdio detect */
	asr_sdio_host_clk_en(false);
	asr_sdio_detect_change();

#ifndef CONFIG_NOT_USED_DTS
#ifdef CONFIG_PINCTRL
	asr_put_pinctrl(dev);
#endif
#endif
	return 0;
}
#endif
//=======================sdio func end========================================

//=======================platform common func start========================================
int asr_platform_power_on(struct device *dev)
{
#ifdef CONFIG_ASR_SDIO
	return asr_sdio_power_on(dev);
#else
	return 0;
#endif
}

int asr_platform_power_off(struct device *dev)
{
#ifdef CONFIG_ASR_SDIO
	return asr_sdio_power_off(dev);
#else
	return 0;
#endif
}

//=======================platform common func end========================================
