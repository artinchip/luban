#ifndef __AIC_BSP_EXPORT_H
#define __AIC_BSP_EXPORT_H

enum aicbsp_subsys {
	AIC_BLUETOOTH,
	AIC_WIFI,
};

enum aicbsp_pwr_state {
	AIC_PWR_OFF,
	AIC_PWR_ON,
};

struct aicbsp_feature_t {
	int      hwinfo;
	uint32_t sdio_clock;
	uint8_t  sdio_phase;
	int fwlog_en;
	uint8_t irqf;
};

enum skb_buff_id {
	AIC_RESV_MEM_TXDATA,
};

#if defined(CONFIG_DPD) || defined(CONFIG_LOFT_CALIB)
typedef struct {
    uint32_t bit_mask[3];
    uint32_t reserved;
    uint32_t dpd_high[96];
    uint32_t dpd_11b[96];
    uint32_t dpd_low[96];
    uint32_t idac_11b[48];
    uint32_t idac_high[48];
    uint32_t idac_low[48];
    uint32_t loft_res[18];
    uint32_t rx_iqim_res[16];
} rf_misc_ram_t;

typedef struct {
    uint32_t bit_mask[4];
    uint32_t dpd_high[96];
    uint32_t loft_res[18];
} rf_misc_ram_lite_t;

#define MEMBER_SIZE(type, member)   sizeof(((type *)0)->member)
#define DPD_RESULT_SIZE_8800DC      sizeof(rf_misc_ram_lite_t)
#endif

#ifdef CONFIG_DPD
extern rf_misc_ram_lite_t dpd_res;
#endif

#ifdef CONFIG_LOFT_CALIB
extern rf_misc_ram_lite_t loft_res_local;
#endif

int aicbsp_set_subsys(int, int);
int aicbsp_get_feature(struct aicbsp_feature_t *feature, char *fw_path);
bool aicbsp_get_load_fw_in_fdrv(void);
struct sk_buff *aicbsp_resv_mem_alloc_skb(unsigned int length, uint32_t id);
void aicbsp_resv_mem_kfree_skb(struct sk_buff *skb, uint32_t id);

#endif
