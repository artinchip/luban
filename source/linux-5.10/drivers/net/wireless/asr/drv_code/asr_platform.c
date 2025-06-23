/**
 ******************************************************************************
 *
 * @file asr_platform.c
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include "hal_desc.h"
#include "asr_main.h"
#include "asr_irqs.h"
#ifdef CONFIG_ASR_SDIO
#include "asr_sdio.h"
#endif
#include "asr_platform.h"
#ifdef CONFIG_ASR_USB
#include "asr_usb.h"
#endif
#include "ipc_host.h"
#include "asr_pm.h"

#ifdef ASR_WIFI_CONFIG_SUPPORT
struct asr_wifi_config g_wifi_config;

struct asr_wifi_config_pwr g_asr_config_pwr[WIFI_CONFIG_MAX_PWR_NUM] = {
	{.cmd_name = WIFI_CONFIG_PWR_1M},
	{.cmd_name = WIFI_CONFIG_PWR_2M},
	{.cmd_name = WIFI_CONFIG_PWR_5M},
	{.cmd_name = WIFI_CONFIG_PWR_11M},
	{.cmd_name = WIFI_CONFIG_PWR_6M},
	{.cmd_name = WIFI_CONFIG_PWR_9M},
	{.cmd_name = WIFI_CONFIG_PWR_12M},
	{.cmd_name = WIFI_CONFIG_PWR_18M},
	{.cmd_name = WIFI_CONFIG_PWR_24M},
	{.cmd_name = WIFI_CONFIG_PWR_36M},
	{.cmd_name = WIFI_CONFIG_PWR_48M},
	{.cmd_name = WIFI_CONFIG_PWR_54M},
	{.cmd_name = WIFI_CONFIG_PWR_MCS0},
	{.cmd_name = WIFI_CONFIG_PWR_MCS1},
	{.cmd_name = WIFI_CONFIG_PWR_MCS2},
	{.cmd_name = WIFI_CONFIG_PWR_MCS3},
	{.cmd_name = WIFI_CONFIG_PWR_MCS4},
	{.cmd_name = WIFI_CONFIG_PWR_MCS5},
	{.cmd_name = WIFI_CONFIG_PWR_MCS6},
	{.cmd_name = WIFI_CONFIG_PWR_MCS7},
	{.cmd_name = WIFI_CONFIG_PWR_HT40_MCS0},
	{.cmd_name = WIFI_CONFIG_PWR_HT40_MCS1},
	{.cmd_name = WIFI_CONFIG_PWR_HT40_MCS2},
	{.cmd_name = WIFI_CONFIG_PWR_HT40_MCS3},
	{.cmd_name = WIFI_CONFIG_PWR_HT40_MCS4},
	{.cmd_name = WIFI_CONFIG_PWR_HT40_MCS5},
	{.cmd_name = WIFI_CONFIG_PWR_HT40_MCS6},
	{.cmd_name = WIFI_CONFIG_PWR_HT40_MCS7},

};
#endif

int asr_config_int_gpio;
int asr_gpio_irq;

extern bool asr_xmit_opt;
extern int downloadfw;
extern int downloadATE;
/**
 * asr_plat_fw_upload - Load the requested FW into embedded side.
 *
 * @asr_plat pointer to platform structure
 * @fw_addr Virtual address where the fw must be loaded
 * @filename Name of the fw.
 *
 * Load a fw, stored as a binary file, into the specified address
 */
#ifdef CONFIG_ASR_SDIO
#define CARD_PWR_UP_BIT    0x10
int asr_sdio_power_up(struct sdio_func *func)
{
	int err_ret = -1;
	int wait_fw_cnt = 0;
	u8 card_int_status = 0;

	sdio_claim_host(func);
	sdio_writeb(func, 0x2, H2C_INTEVENT, &err_ret);	//Host_Pwr_Up ,bit1 .
	sdio_release_host(func);

	if (err_ret == 0)
		dev_info(g_asr_para.dev, "card power_up send!\n");
	else
		dev_err(g_asr_para.dev, "card power_up send fail!\n");

	//wait for fw complete
	do {
		asr_sched_timeout(2);
		wait_fw_cnt++;

		sdio_claim_host(func);
		card_int_status = sdio_readb(func, CARD_INT_STATUS, &err_ret);
		sdio_release_host(func);

		dev_info(g_asr_para.dev, "card int check:%d cnt:%d\n", card_int_status, wait_fw_cnt);

	} while ((card_int_status & CARD_PWR_UP_BIT) && (wait_fw_cnt < 2));

	return err_ret;

}

static int asr_plat_fw_upload(struct asr_plat *asr_plat, char *filename)
{
	const struct firmware *fw_img;
	struct device *dev = asr_platform_get_dev(asr_plat);
	int err = 0;

	err = request_firmware(&fw_img, filename, dev);
	if (err) {
		dev_err(g_asr_para.dev, "Failed to get %s, with error: %d!\n", filename, err);
#ifdef CONFIG_ASR_NO_BOOTROOM
		return 0;
#else
		return err;
#endif
	}
#ifdef CONFIG_ASR_NO_BOOTROOM
	err = 0;
	goto release;
#else
	///send power up evt to card to reinit card before download firmware.
	asr_sdio_power_up(asr_plat->func);
#endif

	/* Copy the file on the Embedded side */
	dev_info(g_asr_para.dev, "### Now copy %s firmware,size is %d\n", filename, (unsigned int)fw_img->size);

	if (downloadATE) {
		//download the ATE firmware
		if (asr_sdio_download_ATE_firmware(asr_plat->func, fw_img) < 0)
			err = -1;
	} else {
#ifdef CONFIG_ASR5825
		if (driver_mode == DRIVER_MODE_ATE) {
			if (asr_sdio_download_ATE_firmware(asr_plat->func, fw_img) < 0)
				err = -1;
		} else {
			//download 2nd bootloader work with romcode v1.0
			if (asr_sdio_download_1st_firmware(asr_plat->func, fw_img) < 0)
				err = -1;
			//download the hybrid firmware work with the 2nd bootloader
			if (asr_sdio_download_2nd_firmware(asr_plat->func, fw_img) < 0)
				err = -1;
		}
#else
		//downlodad firmware using sdio
		if (asr_sdio_download_firmware(asr_plat->func, fw_img) < 0)
			err = -1;
#endif
	}

#ifdef CONFIG_ASR_NO_BOOTROOM
release:
#endif
	release_firmware(fw_img);
	return err;
}

/**
 * asr_plat_lmac_load - Load FW code
 *
 * @asr_plat platform data
 */
int asr_plat_lmac_load(struct asr_plat *asr_plat)
{
	int ret;
	char *filename = NULL;

	if (downloadATE) {
		filename = ASR_ATE_FW_NAME;
	} else if (driver_mode == DRIVER_MODE_ATE) {
		filename = ASR_DRIVER_ATE_FW_NAME;
	} else {
		filename = ASR_MAC_FW_NAME;
	}

	dev_info(&(asr_plat->func->dev), "%s: driver_mode=%d,filename=%s.\n", __func__, driver_mode, filename);

	if (!downloadfw) {
		return 0;
	}

#if defined(CONFIG_ASR595X) && !defined(BASS_SUPPORT)
	ret = asr_plat_fw_upload(asr_plat, ASR_BOOTLD_FW);
	if (ret < 0) {
		dev_err(&(asr_plat->func->dev), "%s: download %s fail.\n", __func__, ASR_BOOTLD_FW);
		return ret;
	}
#endif

	ret = asr_plat_fw_upload(asr_plat, filename);

	return ret;
}


/**
 * asr_platform_on - Start the platform
 *
 * @asr_hw Main driver data
 *
 * It starts the platform :
 * - load fw and ucodes
 * - initialize IPC
 * - boot the fw
 * - enable link communication/IRQ
 *
 * Called by 802.11 part
 */
#ifdef OOB_INTR_ONLY
int asr_init_wifi_oob_intr_gpio(struct asr_plat *asr_plat, struct asr_hw *asr_hw);
#endif
int asr_platform_on(struct asr_hw *asr_hw)
{
	struct asr_plat *asr_plat = asr_hw->plat;
	int ret;

	if (asr_plat->enabled)
		return 0;

	asr_plat->asr_hw = asr_hw;

	sdio_set_drvdata(asr_plat->func, asr_hw);

	if ((ret = asr_ipc_init(asr_hw)))
		return ret;

	//disable host interrupt first, using polling method when download fw
	asr_sdio_disable_int(asr_plat->func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);

	if ((ret = asr_plat_lmac_load(asr_plat)))
		return ret;

	sdio_claim_host(asr_plat->func);
	/*set block size SDIO_BLOCK_SIZE */
	sdio_set_block_size(asr_plat->func, SDIO_BLOCK_SIZE);

	#ifndef OOB_INTR_ONLY
	ret = sdio_claim_irq(asr_plat->func, asr_sdio_isr);
	#else
	ret = 0;
	pr_err("use oob intr only.... \r\n");
	#endif

	sdio_release_host(asr_plat->func);
	if (ret)
		return ret;

	//enable sdio host interrupt
	ret = asr_sdio_enable_int(asr_plat->func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);
	if (ret)
		return ret;

#ifdef CONFIG_POWER_SAVE
	// add power save control gpio.
	ret = asr_init_wifi_wakeup_gpio(asr_plat, asr_hw);
	if (ret < 0)
		return ret;
#endif

#ifdef OOB_INTR_ONLY
	// add out-of-band intr gpio.
	ret = asr_init_wifi_oob_intr_gpio(asr_plat, asr_hw);
	if (ret < 0)
		return ret;
#endif

#ifdef CONFIG_ASR_PM
	ret = asr_pm_init(asr_hw);
	if (ret)
		return ret;
#else
	asr_sdio_set_state(asr_plat, SDIO_STATE_ACTIVE);
#endif

	asr_plat->enabled = true;

	return 0;
}

/**
 * asr_platform_init - Initialize the platform
 *
 * @asr_plat platform data (already updated by platform driver)
 * @platform_data Pointer to store the main driver data pointer (aka asr_hw)
 *                That will be set as driver data for the platform driver
 * @return 0 on success, < 0 otherwise
 *
 * Called by the platform driver after it has been probed
 */
int asr_platform_init(struct asr_plat *asr_plat, void **platform_data)
{
	ASR_DBG(ASR_FN_ENTRY_STR);

	asr_plat->enabled = false;

	return asr_cfg80211_init(asr_plat, platform_data);
}
#endif //CONFIG_ASR_SDIO

#ifdef ASR_WIFI_CONFIG_SUPPORT
static void asr_parsing_config_cmd(struct asr_hw *asr_hw, char *cmd_name, char *value_p)
{
	int index = 0, ret = 0;
	long res = 0;
	char temp_buff[3] = { 0 };
	struct cca_config_info *cca = NULL;

	if (!cmd_name || !value_p) {
		return;
	}
	//dev_info(asr_hw->dev, "%s:\"%s\"=\"%s\"\n", __func__, cmd_name, value_p);

	for (index = 0; index < WIFI_CONFIG_MAX_PWR_NUM; index++) {
		if (!strcmp(cmd_name, g_asr_config_pwr[index].cmd_name)) {
			g_wifi_config.tx_pwr[index] = simple_strtol(value_p, NULL, 10);
			return;
		}
	}

	if (!strcmp(cmd_name, WIFI_CONFIG_MAC)) {
		for (index = 0; index < 6; index++) {
			memcpy(temp_buff, value_p + index * 2, 2);
			ret = kstrtol(temp_buff, 16, &res);
			g_wifi_config.mac_addr[index] = (char)res;
			//dev_info(asr_hw->dev, "temp_buff=%s,ret=%d, buf[%d]=0x%02X\n", temp_buff, ret, index, buf[index]);
		}
		return;
	}

	//CCA
	if (!strncmp(cmd_name, WIFI_CONFIG_CCA_THRESHOLD, 4)) {
		res = simple_strtol(value_p, NULL, 10);
		if (res < 0 && res > -90) {
			cca = &g_wifi_config.cca_config;
			if (!strcmp(cmd_name, WIFI_CONFIG_CCA_THRESHOLD)) {
				cca->cca_valid = true;
				cca->cca_threshold = res;
			} else if (!strcmp(cmd_name, WIFI_CONFIG_CCA_PRISE_THR)) {
				cca->cca_valid = true;
				cca->cca_prise_thr = res;
			} else if (!strcmp(cmd_name, WIFI_CONFIG_CCA_PFALL_THR)) {
				cca->cca_valid = true;
				cca->cca_pfall_thr = res;
			} else if (!strcmp(cmd_name, WIFI_CONFIG_CCA_SRISE_THR)) {
				cca->cca_valid = true;
				cca->cca_srise_thr = res;
			} else if (!strcmp(cmd_name, WIFI_CONFIG_CCA_SFALL_THR)) {
				cca->cca_valid = true;
				cca->cca_sfall_thr = res;
			}
		}
		//dev_info(asr_hw->dev, "%s: CCA(%d,%d,%d,%d,%d,%d)\n", __func__,
		//	cca->cca_valid, cca->cca_threshold, cca->cca_prise_thr, cca->cca_pfall_thr, cca->cca_srise_thr, cca->cca_sfall_thr);
	}

	//EDCA
	if (!strcmp(cmd_name, WIFI_CONFIG_EDCA_BK)) {
		g_wifi_config.edca_bk = simple_strtol(value_p, NULL, 16);
		//dev_info(asr_hw->dev, "%s: EDCA_BK=0x%08X\n", __func__, g_wifi_config.edca_bk);

	} else if (!strcmp(cmd_name, WIFI_CONFIG_EDCA_BE)) {
		g_wifi_config.edca_be = simple_strtol(value_p, NULL, 16);
		//dev_info(asr_hw->dev, "%s: EDCA_BE=0x%08X\n", __func__, g_wifi_config.edca_be);

	} else if (!strcmp(cmd_name, WIFI_CONFIG_EDCA_VI)) {
		g_wifi_config.edca_vi = simple_strtol(value_p, NULL, 16);
		//dev_info(asr_hw->dev, "%s: EDCA_VI=0x%08X\n", __func__, g_wifi_config.edca_vi);

	} else if (!strcmp(cmd_name, WIFI_CONFIG_EDCA_VO)) {
		g_wifi_config.edca_vo = simple_strtol(value_p, NULL, 16);
		//dev_info(asr_hw->dev, "%s: EDCA_VO=0x%08X\n", __func__, g_wifi_config.edca_vo);

	}
}

static void asr_parsing_config(struct asr_hw *asr_hw, char *config_data, int config_len)
{
	char cmd_name[15] = { 0 };
	char cmd_value[15] = { 0 };
	char *cmd_p = NULL, *value_p = NULL;
	char *data_p = config_data;
	int data_len = config_len;
	int cmd_len = 0, value_len = 0;
	int index = 0;

	if (!asr_hw || !config_data || config_len == 0) {
		return;
	}

	while (data_len > 0) {

		if (*data_p == ' ' || *data_p == '\0' || *data_p == '\r' || *data_p == '\n') {
			data_p++;
			data_len--;
		} else {

			cmd_p = strstr(data_p, "=");
			if (!cmd_p) {
				break;
			}

			cmd_len =  cmd_p - data_p;
			if (cmd_len > 0 && cmd_len < 15) {
				strncpy(cmd_name, data_p, cmd_len);
				cmd_name[cmd_len] = '\0';
			}

			data_p = ++cmd_p;
			data_len -= (cmd_len + 1);

			if (data_len <= 0) {
				break;
			}

			value_p = strstr(data_p, "\r");
			if (!value_p) {
				value_p = strstr(data_p, "\n");
				if (!value_p) {
					value_p = data_p + data_len;
				}
			}

			value_len =  value_p - data_p;
			if (value_len > 0 && value_len < 15) {
				strncpy(cmd_value, data_p, value_len);
				cmd_value[value_len] = '\0';
			} else {
				cmd_value[0] = '\0';
			}

			//dev_info(asr_hw->dev, "%s:data_p=%p,value_p=%p,value_len=%d,data_len=%d\n", __func__, data_p, value_p, value_len, data_len);

			data_p = value_p++;
			data_len = config_len - ( data_p -  config_data);

			asr_parsing_config_cmd(asr_hw, cmd_name, cmd_value);
		}
	}

	for (index = 0; index < WIFI_CONFIG_PWR_NUM; index++) {
		if (g_wifi_config.tx_pwr[index] != 0) {
			g_wifi_config.pwr_config = true;

			break;
		}
		//dev_info(asr_hw->dev, "%s:tx_pwr[%d]=%d\n", __func__, index, g_wifi_config.tx_pwr[index]);
	}

	return;
}

int asr_read_wifi_config(struct asr_hw *asr_hw)
{
	int ret = 0;
	const struct firmware *fw_img;
	int config_len = 1024;
	char *rbuff = NULL;

	if (!asr_hw) {
		return -1;
	}

	ret = request_firmware(&fw_img, ASR_CONFIG_FW_NAME, asr_hw->dev);
	if (ret) {
		dev_err(asr_hw->dev, "%s:Failed to get %s, with error: %d!\n", __func__, ASR_CONFIG_FW_NAME, ret);
		return ret;
	}

	dev_info(asr_hw->dev, "%s:parsing file %s, len=%lu!\n", __func__, ASR_CONFIG_FW_NAME, (long unsigned int)fw_img->size);

	if (fw_img->size == 0) {
		release_firmware(fw_img);
		return 0;
	}

	if (fw_img->size < config_len) {
		config_len = fw_img->size;
	}

	rbuff = kmalloc(config_len + 1, GFP_KERNEL);
	if (!rbuff) {
		dev_err(asr_hw->dev, "%s:kmalloc fail,len=%d!\n", __func__, config_len);
		release_firmware(fw_img);
		return 0;
	}

	memset(rbuff, 0, config_len + 1);

	memcpy(rbuff, fw_img->data, config_len);

	//dev_info(asr_hw->dev, "%s:%s,%d:\n%s\n", __func__, ASR_CONFIG_FW_NAME, config_len, rbuff);

	asr_parsing_config(asr_hw, rbuff, config_len);

	kfree(rbuff);
	release_firmware(fw_img);
	return 0;
}
#endif

/**
 * asr_platform_deinit - Deinitialize the platform
 *
 * @asr_hw ain driver data
 *
 * Called by the platform driver after it is removed
 */
void asr_platform_deinit(struct asr_hw *asr_hw)
{
	ASR_DBG(ASR_FN_ENTRY_STR);

	asr_cfg80211_deinit(asr_hw);
}

long asr_irq_times;
#ifdef CONFIG_POWER_SAVE
irqreturn_t asr_asr_gpio_wakeup_isr(int irq, void *dev_instance)
{
	//struct net_device *dev = NULL;
	struct asr_hw *asr_hw = NULL;

	asr_hw = (struct asr_hw *)dev_instance;

	asr_irq_times++;

#if 0
	if (asr_hw->pwr_state == RTW_STS_SUSPEND) {
		DEBUG_INFO("[%s,%d] RTW_STS_SUSPEND\n", __FUNCTION__, __LINE__);
		asr_hw->pwr_state = RTW_STS_NORMAL;
		asr_hw->ps_ctrl = RTW_ACT_IDLE;

		schedule_work(&asr_hw->ap_cmd_queue);
	}
#endif

	return IRQ_HANDLED;
}

int asr_set_asr_wakeup_pin(struct asr_plat *asr_plat, struct asr_hw *asr_hw)
{
	struct device_node *np = NULL;
	//struct property *prop;
	int rc, gpio;

	np = of_find_node_by_name(NULL, "asr-rfkill");
	if (!np) {
		np = of_find_node_by_name(NULL, "sd8x-rfkill");
		if (!np) {
			printk("error: no gpio for wifi wakeup in dts\n");
			return -1;
		}
	}

	gpio = of_get_named_gpio(np, "edge-wakeup-gpio", 0);
	if (unlikely(gpio < 0)) {
		printk("err: set_asr_wakeup_pin: edge-wakeup-gpio undefined\n");
		return -1;
	}

	asr_hw->wakeup_gpio = gpio;
	printk("edge-wakeup-gpio pin found %d\n", gpio);

	rc = gpio_request(asr_hw->wakeup_gpio, "wlan wakup pin");
	if (rc) {
		printk("err: %s: gpio_request %d failed\n", __func__, asr_hw->wakeup_gpio);
		return rc;
	}

	rc = gpio_direction_input(asr_hw->wakeup_gpio);
	if (rc) {
		printk("err: %s: gpio_direction_input %d failed\n", __func__, asr_hw->wakeup_gpio);
		return rc;
	}

	asr_plat->irq = gpio_to_irq(asr_hw->wakeup_gpio);
	if (asr_plat->irq < 0) {
		printk("err: %s: gpio_to_irq %d failed\n", __func__, asr_hw->wakeup_gpio);
		return rc;
	}
	//asr_hw->wake_irq = asr_plat->irq;
	rc = request_irq(asr_plat->irq, asr_asr_gpio_wakeup_isr,
			 IRQF_SHARED | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "asr", asr_hw);

	if (rc) {
		printk("some issue in wake-up irq, rx=%d\n", rc);
		return -1;
	}

	return rc;
}

int asr_init_wifi_wakeup_gpio(struct asr_plat *asr_plat, struct asr_hw *asr_hw)
{
	int err = 0;
	printk("[%s] ENTRY \n", __FUNCTION__);

	err = asr_set_asr_wakeup_pin(asr_plat, asr_hw);

	return err;
}
#endif


#ifdef OOB_INTR_ONLY
int asr_set_asr_oob_intr_pin(struct asr_plat *asr_plat, struct asr_hw *asr_hw)
{
	int rc = -1;

	asr_hw->oob_intr_gpio = g_asr_para.oob_intr_pin;

	printk("oob intr pin found %d\n", asr_hw->oob_intr_gpio);

	asr_plat->oob_irq  = gpio_to_irq(asr_hw->oob_intr_gpio);
	if (asr_plat->oob_irq  < 0) {
		printk(("%s irq information is incorrect\n", __FUNCTION__));
		return -1;
	}

	/* sched_setscheduler on ONESHOT threaded irq handler for BCNs ? */
	rc = request_irq(asr_plat->oob_irq, asr_oob_irq_hdlr,
			 IRQF_SHARED | IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "asr", asr_hw);

	if (rc) {
		printk("some issue in oob intr irq, rx=%d\n", rc);
		return -1;
	}

	return rc;
}

int asr_init_wifi_oob_intr_gpio(struct asr_plat *asr_plat, struct asr_hw *asr_hw)
{
	int err = 0;
	printk("[%s] ENTRY \n", __FUNCTION__);

	err = asr_set_asr_oob_intr_pin(asr_plat, asr_hw);

	return err;
}
#endif


/**
 * asr_platform_off - Stop the platform
 *
 * @asr_hw Main driver data
 *
 * Called by 802.11 part
 */
extern void asr_txhifbuffs_dealloc(struct asr_hw *asr_hw);

void asr_platform_off(struct asr_hw *asr_hw)
{
#ifdef CONFIG_ASR_SDIO
	struct asr_plat *asr_plat = asr_hw->plat;
	int ret = 0;

	if (!asr_plat->enabled)
		return;

	asr_sdio_disable_int(asr_plat->func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);

	sdio_claim_host(asr_plat->func);
	ret = sdio_release_irq(asr_plat->func);
	sdio_release_host(asr_plat->func);
	if (ret) {
		dev_err(g_asr_para.dev, "ERROR: release sdio irq fail %d.\n", ret);
	} else {
		dev_info(g_asr_para.dev, "ASR: release sdio irq success.\n");
	}
#endif

#ifdef CONFIG_POWER_SAVE
	if (asr_plat->irq)
		free_irq(asr_plat->irq, asr_hw);

	gpio_free(asr_hw->wakeup_gpio);
#endif

#ifdef OOB_INTR_ONLY
	if (asr_plat->oob_irq)
		free_irq(asr_plat->oob_irq, asr_hw);

	gpio_free(asr_hw->oob_intr_gpio);
#endif

	asr_ipc_deinit(asr_hw);

#ifdef CONFIG_ASR_SDIO

	if (asr_xmit_opt) {
		asr_txhifbuffs_dealloc(asr_hw);
	} else {
		dev_kfree_skb(asr_hw->tx_agg_env.aggr_buf);
		memset(&asr_hw->tx_agg_env, 0, sizeof(asr_hw->tx_agg_env));
	}

	asr_plat->enabled = false;
	sdio_set_drvdata(asr_plat->func, NULL);
#endif

#ifdef CONFIG_ASR_PM
	asr_pm_deinit(asr_hw);
#endif
}

static int asr_platform_probe(struct platform_device *pdev)
{
	int ret;
#ifdef	CONFIG_ASR_USB
	g_asr_para.dev_driver_remove_cnt++;
	if (g_asr_para.dev_driver_remove == false) {
		dev_err(&pdev->dev, "%s %d cnt:%d\r\n", __func__, g_asr_para.dev_driver_remove,
			g_asr_para.dev_driver_remove_cnt);
		return 0;
	}
#endif
	g_asr_para.dev_driver_remove = false;
	g_asr_para.dev = &pdev->dev;

	dev_info(&pdev->dev, "%s: start\n", __func__);

	asr_platform_power_on(&pdev->dev);

#ifdef CONFIG_ASR_SDIO
	ret = asr_sdio_register_drv();
	if (ret) {
		dev_err(&pdev->dev, "%s: asr_sdio_register_drv fail\n", __func__);
		asr_platform_power_off(&pdev->dev);
		return ret;
	}
#endif

#ifdef CONFIG_ASR_USB
	ret = asr_usb_register_drv();
	if (ret) {
		dev_err(&pdev->dev, "%s: asr_usb_register_drv fail\n", __func__);
		return ret;
	}
#endif

	/*
	 * just set a arbitrary value, so we can check it in
	 * asr_platform_remove().
	 */
	platform_set_drvdata(pdev, pdev);

	return 0;
}

static int asr_platform_remove(struct platform_device *pdev)
{
	void *data;
#ifdef CONFIG_ASR_USB
	g_asr_para.dev_driver_remove_cnt--;
	if (g_asr_para.dev_driver_remove == true) {
		dev_err(&pdev->dev, "%s %d cnt:%d\r\n", __func__, g_asr_para.dev_driver_remove,
			g_asr_para.dev_driver_remove_cnt);
		return 0;
	}
#endif
	g_asr_para.dev_driver_remove = true;

	data = platform_get_drvdata(pdev);
	if (data == NULL)
		return -ENODEV;
	dev_info(&pdev->dev, "%s: start\n", __func__);

#ifdef CONFIG_ASR_SDIO
	asr_sdio_unregister_drv();
#endif

#ifdef CONFIG_ASR_USB
	dev_info(&pdev->dev, "%s exit\n", __func__);
	asr_usb_exit();
#endif

	asr_platform_power_off(&pdev->dev);

	return 0;
}

static const struct of_device_id asr_of_match[] = {
	{
	 .compatible = "asr,asr-platform",
	 },
	{},
};

static struct platform_driver asr_platform_driver = {
	.probe = asr_platform_probe,
	.remove = asr_platform_remove,
	.driver = {
		   .name = "asr-platform",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = asr_of_match,
#endif
		   },
};

/**
 * asr_platform_register_drv - Register all possible platform drivers
 */
int asr_platform_register_drv(void)
{
	/*
	 ** Before load asr_fdrv.ko,
	 ** for fpga device, no need to do chip reset(usually use gpio), no need clk and power init
	 **     so when host device init(mmc controller), it can detect sdio card device
	 ** for soc chip, may need gpio reset/clk/power init before mmc controller init
	 ** this only for sdio card(wifi chip) configuration, mainly gpio interrupt init
	 */

	return platform_driver_register(&asr_platform_driver);
}

/**
 * asr_platform_unregister_drv - Unegister all platform drivers
 */
void asr_platform_unregister_drv(void)
{
	platform_driver_unregister(&asr_platform_driver);
}

MODULE_FIRMWARE(ASR_AGC_FW_NAME);
MODULE_FIRMWARE(ASR_FCU_FW_NAME);
MODULE_FIRMWARE(ASR_LDPC_RAM_NAME);
MODULE_FIRMWARE(ASR_MAC_FW_NAME);
