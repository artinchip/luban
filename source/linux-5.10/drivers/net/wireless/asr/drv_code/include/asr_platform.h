/**
 ******************************************************************************
 *
 * @file asr_platorm.h
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */

#ifndef _ASR_PLAT_H_
#define _ASR_PLAT_H_

#define ASR_FW_DIR_NAME                "/lib/firmware"
#define ASR_CONFIG_FW_NAME             "asr_wifi_config.ini"	//WIFI CONFIG
#define ASR_PHY_CONFIG_TRD_NAME        "asr_trident.ini"	// null
#define ASR_PHY_CONFIG_KARST_NAME      "asr_karst.ini"	//0100 0000
#define ASR_AGC_FW_NAME                "agcram.bin"
#define ASR_LDPC_RAM_NAME              "ldpcram.bin"

#ifdef CONFIG_ASR5531

#ifdef CONFIG_ASR_SDIO
#define ASR_MAC_FW_NAME        "fmacfw_asr5532s_sdio.bin"
#define ASR_ATE_FW_NAME        "ate_asr5531_sdio.bin"
#define ASR_DRIVER_ATE_FW_NAME "ate_sdio_asr5531.bin"
#else
#define ASR_MAC_FW_NAME        "fmacfw_asr5532u_usb.bin"
#define ASR_ATE_FW_NAME        "ate_asr5531_usb.bin"
#define ASR_DRIVER_ATE_FW_NAME "ate_usb_asr5531.bin"
#endif

#elif defined(CONFIG_ASR5505)

#define ASR_MAC_FW_NAME            "fmacfw_asr5505.bin"

#define ASR_ATE_FW_NAME            "ate_asr5505.bin"
#define ASR_DRIVER_ATE_FW_NAME     "ate_sdio_asr5505.bin"

#elif defined(CONFIG_ASR5825)

#define ASR_MAC_FW_NAME            "fmacfw_asr5825.bin"

#define ASR_ATE_FW_NAME            "ate_asr5825.bin"
#define ASR_DRIVER_ATE_FW_NAME     "ate_sdio_asr5825.bin"

#elif defined(CONFIG_ASR595X) && !defined(BASS_SUPPORT)

#define ASR_BOOTLD_FW              "bootld_asr595x.bin"

#define ASR_MAC_FW_NAME            "fmacfw_asr595x.bin"

#define ASR_ATE_FW_NAME            "ate_asr595x.bin"
#define ASR_DRIVER_ATE_FW_NAME     "ate_sdio_asr595x.bin"

#elif defined(BASS_SUPPORT)

#define ASR_MAC_FW_NAME            "fmacfw_asr596x.bin"

#define ASR_ATE_FW_NAME            "ate_asr596x.bin"
#define ASR_DRIVER_ATE_FW_NAME     "ate_sdio_asr596x.bin"

#else
#define ASR_MAC_FW_NAME                "fmacfw.bin"
#define ASR_ATE_FW_NAME                "ate.bin"
#define ASR_DRIVER_ATE_FW_NAME         "ate_hif.bin"

#endif

#define ASR_FCU_FW_NAME                "fcuram.bin"

#define WIFI_CONFIG_PWR_1M          "PWR_1M"
#define WIFI_CONFIG_PWR_2M          "PWR_2M"
#define WIFI_CONFIG_PWR_5M          "PWR_5M"
#define WIFI_CONFIG_PWR_11M         "PWR_11M"
#define WIFI_CONFIG_PWR_6M          "PWR_6M"
#define WIFI_CONFIG_PWR_9M          "PWR_9M"
#define WIFI_CONFIG_PWR_12M         "PWR_12M"
#define WIFI_CONFIG_PWR_18M         "PWR_18M"
#define WIFI_CONFIG_PWR_24M         "PWR_24M"
#define WIFI_CONFIG_PWR_36M         "PWR_36M"
#define WIFI_CONFIG_PWR_48M         "PWR_48M"
#define WIFI_CONFIG_PWR_54M         "PWR_54M"
#define WIFI_CONFIG_PWR_MCS0        "PWR_MCS0"
#define WIFI_CONFIG_PWR_MCS1        "PWR_MCS1"
#define WIFI_CONFIG_PWR_MCS2        "PWR_MCS2"
#define WIFI_CONFIG_PWR_MCS3        "PWR_MCS3"
#define WIFI_CONFIG_PWR_MCS4        "PWR_MCS4"
#define WIFI_CONFIG_PWR_MCS5        "PWR_MCS5"
#define WIFI_CONFIG_PWR_MCS6        "PWR_MCS6"
#define WIFI_CONFIG_PWR_MCS7        "PWR_MCS7"
#define WIFI_CONFIG_PWR_HT40_MCS0   "PWR_HT40_MCS0"
#define WIFI_CONFIG_PWR_HT40_MCS1   "PWR_HT40_MCS1"
#define WIFI_CONFIG_PWR_HT40_MCS2   "PWR_HT40_MCS2"
#define WIFI_CONFIG_PWR_HT40_MCS3   "PWR_HT40_MCS3"
#define WIFI_CONFIG_PWR_HT40_MCS4   "PWR_HT40_MCS4"
#define WIFI_CONFIG_PWR_HT40_MCS5   "PWR_HT40_MCS5"
#define WIFI_CONFIG_PWR_HT40_MCS6   "PWR_HT40_MCS6"
#define WIFI_CONFIG_PWR_HT40_MCS7   "PWR_HT40_MCS7"
#define WIFI_CONFIG_MAC             "MAC"
#define WIFI_CONFIG_CCA_THRESHOLD   "CCA_THRESHOLD"
#define WIFI_CONFIG_CCA_PRISE_THR   "CCA_PRISETHR"
#define WIFI_CONFIG_CCA_PFALL_THR   "CCA_PFALLTHR"
#define WIFI_CONFIG_CCA_SRISE_THR   "CCA_SRISETHR"
#define WIFI_CONFIG_CCA_SFALL_THR   "CCA_SFALLTHR"
#define WIFI_CONFIG_EDCA_BK         "EDCA_BK"
#define WIFI_CONFIG_EDCA_BE         "EDCA_BE"
#define WIFI_CONFIG_EDCA_VI         "EDCA_VI"
#define WIFI_CONFIG_EDCA_VO         "EDCA_VO"

#define WIFI_CONFIG_MAX_PWR_NUM     28

#define PM_HIGH_LEVEL 1
#define PM_LOW_LEVEL  0

/**
 * SDIO bus state
 * SDIO_STATE_DOWNLOAD
   Indicate the card needs to download firmware to access
 * SDIO_STATE_ACTIVE
 * SDIO_STATE_RESUMING
 * SDIO_STATE_SUSPENDING
 * SDIO_STATE_SUSPENDED
 * SDIO_STATE_POWEROFF
 */
enum asr_sdio_state {
	SDIO_STATE_POWEROFF,
	SDIO_STATE_DOWNLOAD,
	SDIO_STATE_ACTIVE,
	SDIO_STATE_RESUMING,
	SDIO_STATE_SUSPENDING,
	SDIO_STATE_SUSPENDED,
};

struct asr_wifi_config_pwr {
	s8 *cmd_name;
};

struct asr_hw;

/**
 * @pci_dev pointer to pci dev
 * @enabled Set if embedded platform has been enabled (i.e. fw loaded and
 *          ipc started)
 * @enable Configure communication with the fw (i.e. configure the transfers
 *         enable and register interrupt)
 * @disable Stop communication with the fw
 * @deinit Free all ressources allocated for the embedded platform
 * @get_address Return the virtual address to access the requested address on
 *              the platform.
 * @ack_irq Acknowledge the irq at link level.
 *
 * @priv Private data for the link driver
 */
struct asr_plat {
	struct sdio_func *func;
	struct asr_hw *asr_hw;
	bool enabled;
	/*
	 * Instead of touching interrupt line and base address registers
	 * directly, use the values stored here. They might be different!
	 */
#ifdef OOB_INTR_ONLY
	unsigned int oob_irq;
#endif
	volatile enum asr_sdio_state state;

#ifdef CONFIG_ASR_PM
#ifdef CONFIG_GPIO_WAKEUP_MOD
	int pm_out_gpio;
#endif /* CONFIG_GPIO_WAKEUP_MOD */

#ifdef CONFIG_GPIO_WAKEUP_HOST
	unsigned int pm_irq;
	int pm_in_gpio;
	int active_level;
#endif /* CONFIG_GPIO_WAKEUP_HOST */

	struct workqueue_struct *pm_workq;

	struct delayed_work polling_work;
	int polling_cnt;
	int rescan_disable; //record the rescan_disable of mmc host

	spinlock_t pm_lock;
	struct delayed_work pm_cmd_work;
	volatile int pm_cnt;

#ifdef CONFIG_AUTO_SUSPEND_RESUME
	volatile int async_resume;
	struct delayed_work auto_suspend;
	wait_queue_head_t pm_waitq;
	unsigned long suspend_delay;
#endif
#endif /* CONFIG_ASR_PM */
};

int asr_platform_init(struct asr_plat *asr_plat, void **platform_data);
void asr_platform_deinit(struct asr_hw *asr_hw);

int asr_platform_on(struct asr_hw *asr_hw);
void asr_platform_off(struct asr_hw *asr_hw);

int asr_platform_register_drv(void);
void asr_platform_unregister_drv(void);

#ifdef CONFIG_ASR_SDIO
static inline struct device *asr_platform_get_dev(struct asr_plat *asr_plat)
{
	return &(asr_plat->func->dev);
}
#endif

int asr_platform_power_on(struct device *dev);
int asr_platform_power_off(struct device *dev);

int asr_plat_lmac_load(struct asr_plat *asr_plat);
int asr_set_wifi_reset(struct device *dev, u32 delay_ms);
void asr_sdio_detect_change(void);

#ifdef ASR_WIFI_CONFIG_SUPPORT
int asr_read_wifi_config(struct asr_hw *asr_hw);
#endif

#endif /* _ASR_PLAT_H_ */
