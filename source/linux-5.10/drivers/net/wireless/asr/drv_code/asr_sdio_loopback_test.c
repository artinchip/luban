/*
 * ASR SDIO driver
 *
 * Copyright (C) 2017, ASRmicro Technology.
 *
 * This software file (the "File") is distributed by Marvell International
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991
 * (the "License").  You may use, redistribute and/or modify this File in
 * accordance with the terms and conditions of the License, a copy of which
 * is available by writing to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 * worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 * this warranty disclaimer.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <net/sock.h>
#include <net/lib80211.h>
#include <linux/firmware.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
//#include <asm/mach/arch.h>
//#include <asm/mach/irq.h>
#include "asr_platform.h"
#include "asr_defs.h"
#include <linux/clk.h>
#ifdef ASR_MACH_PXA1826_CLK_EN
#include <linux/platform_data/pxa_sdhci.h>
#endif
#include "../../../../mmc/host/sdhci.h"

#define H2C_INTEVENT	0x0
#define HOST_INT_RSR	0x1
#define HOST_INTMASK	0x2
#define HOST_INT_UPLD_EN		(1 << 0)
#define HOST_INT_DNLD_EN		(1 << 1)
#define HOST_INT_UNDER_FLOW_EN	(1 << 2)
#define HOST_INT_OVER_FLOW_EN	(1 << 3)
#define HOST_INT_MASK	(HOST_INT_UPLD_EN | HOST_INT_DNLD_EN | HOST_INT_UNDER_FLOW_EN | HOST_INT_OVER_FLOW_EN)
#define HOST_INT_STATUS	0x3
#define HOST_INT_UPLD_ST		(1 << 0)
#define HOST_INT_DNLD_ST		(1 << 1)
#define HOST_INT_UNDER_FLOW_ST	(1 << 2)
#define HOST_INT_OVER_FLOW_ST	(1 << 3)
#define RD_BITMAP_L		0x4
#define RD_BITMAP_U		0x5
#define WR_BITMAP_L		0x6
#define WR_BITMAP_U		0x7

#define RD_LEN0_L		0x8
#define RD_LEN0_U		0x9
#define RD_LEN1_L		0xA
#define RD_LEN1_U		0xB
#define RD_LEN2_L		0xC
#define RD_LEN2_U		0xD
#define RD_LEN3_L		0xE
#define RD_LEN3_U		0xF
#define RD_LEN4_L		0x10
#define RD_LEN4_U		0x11
#define RD_LEN5_L		0x12
#define RD_LEN5_U		0x13
#define RD_LEN6_L		0x14
#define RD_LEN6_U		0x15
#define RD_LEN7_L		0x16
#define RD_LEN7_U		0x17
#define RD_LEN8_L		0x18
#define RD_LEN8_U		0x19
#define RD_LEN9_L		0x1A
#define RD_LEN9_U		0x1B
#define RD_LEN10_L		0x1C
#define RD_LEN10_U		0x1D
#define RD_LEN11_L		0x1E
#define RD_LEN11_U		0x1F
#define RD_LEN12_L		0x20
#define RD_LEN12_U		0x21
#define RD_LEN13_L		0x22
#define RD_LEN13_U		0x23
#define RD_LEN14_L		0x24
#define RD_LEN14_U		0x25
#define RD_LEN15_L		0x26
#define RD_LEN15_U		0x27

#define RD_LEN_L(index)	(0x8 + (index) * 2)
#define RD_LEN_U(index)	(0x9 + (index) * 2)

#define C2H_INTEVENT	0x30
#define C2H_DNLD_CARD_RDY	(1 << 0)
#define C2H_UPLD_CARD_RDY	(1 << 1)
#define C2H_CIS_CARD_RDY	(1 << 2)
#define C2H_IO_RDY			(1 << 3)

#define RD_BASE_0		0x40
#define RD_BASE_1		0x41
#define RD_BASE_2		0x42
#define RD_BASE_3		0x43
#define WR_BASE_0		0x44
#define WR_BASE_1		0x45
#define WR_BASE_2		0x46
#define WR_BASE_3		0x47
#define RD_INDEX		0x48
#define WR_INDEX		0x49

#define SCRATCH_0		0x60
#define SCRATCH_1		0x61
#define CRC_SUCCESS		(1 << 14)
//#define CRC_SUCCESS		0xABCD
#define CRC_FAIL		0xABCE
//#define BOOT_SUCCESS	0xABCF
#define BOOT_SUCCESS	(1 << 15)

#define IO_PORT_0		0x78
#define IO_PORT_1		0x79
#define IO_PORT_2		0x7a

#define MAX_POLL_TRIES	100
#define SDIO_BLOCK_SIZE	256
/** Macros for Data Alignment : size */
#define ALIGN_SZ(p, a)  \
	(((p) + ((a) - 1)) & ~((a) - 1))

/** Macros for Data Alignment : address */
#define ALIGN_ADDR(p, a)    \
	((((unsigned int)(p)) + (((unsigned int)(a)) - 1)) & ~(((unsigned int)(a)) - 1))
#define WLAN_UPLD_SIZE                  (2312)
#define DMA_ALIGNMENT            32
static char *fw_name = NULL;
static int is_irq = 0;
static int loopback_times = 1;
static int loopback_test_mode = 0x7;
static int direct_block_size = 0;
static bool is_loopback_test = 0;
static int func_number = 0;
static bool is_loopback_display = 0;
static bool direct_pass = 0;
static int crc_value = 0;
static int is_host_int = 0;
static int test_irq_count = 100;
int card_indication_gpio;
int test_gpio;
int card_irq;
static int gpio_irq_times;
DECLARE_COMPLETION(dnld_completion);
DECLARE_COMPLETION(upld_completion);
static DEFINE_MUTEX(dnld_mutex);
static DEFINE_MUTEX(upld_mutex);
typedef struct port_buf{
	void *buf;
	int len;
} port_buf_t;
port_buf_t *port_buf[16];
/*every time set_block_size need update this variable*/
static int g_block_size = 0;
struct asr_global_param g_asr_sdio_para;


static void asr_sdio_host_clk_en(bool clk_en);

static uint32_t asr_crc32(const uint8_t *buf, uint32_t len)
{
	uint32_t i;
	uint32_t crc = 0xffffffff;

	while (len--) {
		crc ^= *buf++;
		for (i = 0; i < 8; ++i) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320;
			else
				crc = (crc >> 1);
		}
	}
	return ~crc;
}

/*
	poll_card_status: check if card  in ready state
*/
static int poll_card_status(struct sdio_func *func, uint8_t mask)
{
	uint32_t tries;
	u8 reg_val;
	int ret;

	for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
		sdio_claim_host(func);
		reg_val = sdio_readb(func, C2H_INTEVENT, &ret);
		sdio_release_host(func);
		if (ret) {
			break;
		}
		if (direct_pass)
			return 0;
		pr_info("%s reg_val:0x%x mask:0x%x\n", __func__, reg_val, mask);
		if ((reg_val & mask) == mask)
			return 0;
		msleep(1);
	}
    pr_info("%s can't poll status %d out\n", __func__, C2H_INTEVENT);

	return -1;
}

/*
	check_scratch_status: check scratch register value
*/
static int check_scratch_status(struct sdio_func *func, u16 status)
{
	uint32_t tries;
	//u8 reg_val;
	u16 scratch_val;
	int ret;

	for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
		//read 1 time 2 bytes
		sdio_claim_host(func);
		scratch_val = sdio_readw(func, SCRATCH_0, &ret);
		sdio_release_host(func);
		if (ret) {
			pr_info("%s can't read %d out\n", __func__, C2H_INTEVENT);
			break;
		}
		pr_info("%s scratch_val:0x%x status:0x%x\n", __func__, scratch_val, status);
		if (scratch_val & status)
			return 0;//success
		udelay(2);
		//read 2 times 1 byte
		/*
		sdio_claim_host(func);
		reg_val = sdio_readb(func, SCRATCH_0, &ret);
		sdio_release_host(func);
		if (ret) {
			pr_info("%s can't read %d out\n", __func__, C2H_INTEVENT);
			break;
		}
		scratch_val = reg_val & 0xff;
		sdio_claim_host(func);
		reg_val = sdio_readb(func, SCRATCH_1, &ret);
		sdio_release_host(func);
		if (ret) {
			pr_info("%s can't read %d out\n", __func__, C2H_INTEVENT);
			break;
		}
		scratch_val |= ((reg_val & 0xff) << 8);
		pr_info("%s scratch_val:0x%x status:0x%x\n", __func__, scratch_val, status);
		if (scratch_val == status)
			return 0;//success
		udelay(10);
		*/
	}
	return -1;//fail
}

/*
	get_ioport: get ioport value
*/
static uint32_t get_ioport(struct sdio_func *func)
{
	int ret;
	u8 reg;
	uint32_t ioport;

	/*get out ioport value*/
	sdio_claim_host(func);
	reg = sdio_readb(func, IO_PORT_0, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s can't read %d\n", __func__, IO_PORT_0);
		goto exit;
	}
	ioport = reg & 0xff;

	sdio_claim_host(func);
	reg = sdio_readb(func, IO_PORT_1, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s can't read %d\n", __func__, IO_PORT_1);
		goto exit;
	}
	ioport |= ((reg & 0xff) << 8);

	sdio_claim_host(func);
	reg = sdio_readb(func, IO_PORT_2, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s can't read %d\n", __func__, IO_PORT_2);
		goto exit;
	}
	ioport |= ((reg & 0xff) << 16);
	//pr_info("%s ioport:0x%x default value should be 0x%x\n", __func__, ioport, 0x10000);

	return ioport;
exit:
	return -1;
}

/*
	asr_sdio_enable_int: enable host interrupt
*/
static int asr_sdio_enable_int(struct sdio_func *func, int mask)
{
	uint8_t reg_val;
	int ret = -1;

	if (!(mask & HOST_INT_MASK)) {
		pr_info("%s 0x%x is invalid int mask\n", __func__, mask);
		return ret;
	}
#if 0
	/*clear all interrupts*/
	/*0 write INT RSR*/
	sdio_claim_host(func);
	sdio_writeb(func, HOST_INT_MASK, HOST_INT_RSR, &ret);
	sdio_release_host(func);

	/*read host int status*/
	sdio_claim_host(func);
	reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
	sdio_release_host(func);
	reg_val &= ~HOST_INT_MASK;

	/*2write host int status*/
	sdio_claim_host(func);
	sdio_writeb(func, reg_val, HOST_INT_STATUS, &ret);
	sdio_release_host(func);

#endif
	/*enable specific interrupt*/
	sdio_claim_host(func);
	reg_val = sdio_readb(func, HOST_INTMASK, &ret);
	sdio_release_host(func);
	if (ret)
		return ret;
	reg_val |= (mask & HOST_INT_MASK);
	sdio_claim_host(func);
	sdio_writeb(func, reg_val, HOST_INTMASK, &ret);
	sdio_release_host(func);

	return ret;
}

/*
	download_firmware: download firmware into card
	total data format will be: | header data | fw_data | padding | CRC |(CRC check fw_data + padding)
	header data will be: | fw_len | transfer unit | transfer times |
*/
static int download_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	int ret;
	uint32_t header_data[3];
	uint32_t ioport = 0;
	uint32_t fw_crc;
	uint32_t fw_len, total_len, pad_len, crc_len, tx_len, offset;
	u8 *fw_buf, *fw;
	u8 *tempbuf = NULL;
	uint32_t tempsz;
	u8 padding_data[4] = {0};
	u8 *temp_fw_crc;
	int retry_times = 3;
	int send_time = 0;

	ioport = get_ioport(func);
	if (ioport < 0) {
		pr_info("%s get ioport failed\n", __func__);
		goto exit;
	}

	/*step 0 prepare header data and other initializations*/
	fw_len = fw_img->size;
	pad_len = (fw_len % 4) ? (4 - (fw_len % 4)) : 0;
	crc_len = 4;
	total_len = fw_len + pad_len + crc_len;
	header_data[0] = fw_img->size + pad_len;
	header_data[1] = 2 * SDIO_BLOCK_SIZE;
	header_data[2] = (total_len + (2 * SDIO_BLOCK_SIZE - 1)) / (2 * SDIO_BLOCK_SIZE);
	tempsz = ALIGN_SZ(WLAN_UPLD_SIZE, DMA_ALIGNMENT);
	tempbuf = devm_kzalloc(&func->dev, tempsz * DMA_ALIGNMENT, GFP_KERNEL | GFP_DMA);
	if (!tempbuf) {
		pr_info("%s can't alloc %d memory\n", __func__, tempsz * DMA_ALIGNMENT);
		goto exit;
	}
	fw_buf = (u8 *)(ALIGN_ADDR((unsigned int)tempbuf, DMA_ALIGNMENT));
	/*collect padding and crc data into one place*/
	fw = devm_kzalloc(&func->dev, total_len, GFP_KERNEL);
	if (!fw) {
		pr_info("%s can't alloc %d memory\n", __func__, total_len);
		goto exit;
	}
	temp_fw_crc = devm_kzalloc(&func->dev, crc_len, GFP_KERNEL);
	if (!temp_fw_crc) {
		goto exit;
	}
retry:
	if (!retry_times)
		goto exit;
	pr_info("%s retry %d times\n", __func__, 3 - retry_times);
	retry_times--;
	{
		tx_len = 2 * SDIO_BLOCK_SIZE;
		offset = 0;
		memmove(fw_buf, header_data, sizeof(header_data));
		memcpy(fw, fw_img->data, fw_img->size);
		memcpy(fw + fw_img->size, padding_data, pad_len);
		/*calculate CRC, data parts include the padding data*/
		if (crc_value == 0)
			fw_crc = asr_crc32(fw, fw_img->size + pad_len);
		else {
			fw_crc = crc_value;
			crc_value = 0;//after retry ignore using user-defined crc value
		}
		if (is_host_int == 1) {
			send_time = 0;
		}
		*(uint32_t *)(temp_fw_crc) = fw_crc;
		memcpy(fw + fw_img->size + pad_len, (u8 *)temp_fw_crc, crc_len);
		pr_info("%s fw_len:%d pad_len:%d crc_len:%d total_len:%d headers: 0x%x-0x%x-0x%x crc:0x%x\n",
				__func__, fw_len, pad_len, crc_len, total_len, header_data[0], header_data[1], header_data[2], fw_crc);
	}
	/*step 1 polling if card is in rdy*/
	ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
	if (ret) {
		pr_info("%s card is not in rdy\n", __func__);
		goto exit;
	}

	/*step 2 send out header data*/
	if (is_irq)
		mutex_lock(&dnld_mutex);
	sdio_claim_host(func);
	ret = sdio_writesb(func, ioport, fw_buf, sizeof(header_data));
	sdio_release_host(func);
	if (ret) {
		if (is_irq)
			mutex_unlock(&dnld_mutex);
		pr_info("%s write header data failed %d\n", __func__, ret);
		goto exit;
	}
	gpio_irq_times = 0;
	/*step 3 send out fw data*/
	do {
		if (is_irq) {
			reinit_completion(&dnld_completion);
			mutex_unlock(&dnld_mutex);
			//asr_sdio_enable_int(func, HOST_INT_UPLD_EN);
			if (!wait_for_completion_timeout(&dnld_completion, msecs_to_jiffies(5000))) {
				ret = -ETIMEDOUT;
				goto exit;
			}
		} else {
			ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
			if (ret) {
				pr_info("%s card is not in rdy\n", __func__);
				goto exit;
			}
		}
		if (total_len && total_len - offset < tx_len)
			tx_len = total_len - offset;
		else
			tx_len = 2 * SDIO_BLOCK_SIZE;
		memmove(fw_buf, fw + offset, tx_len);
		/*set an register to let sdio send interrupt to cm4*/
		if (is_host_int == 1) {
			send_time++;
			if (send_time == header_data[2] / 2) {
				sdio_claim_host(func);
				sdio_writeb(func, 0x8, H2C_INTEVENT, &ret);
				sdio_release_host(func);
				pr_info("%s write 0x%x into 0x%x\n", __func__, 0x10, H2C_INTEVENT);
				msleep(100);
				if (ret) {
					pr_info("%s can't write into card %d\n", __func__, ret);
					goto exit;
				}
				is_host_int = 0;//after retry ignore send host int
				goto retry;
			}
		}
		if (is_irq)
			mutex_lock(&dnld_mutex);
		sdio_claim_host(func);
		ret = sdio_writesb(func, ioport, fw_buf, tx_len);
		sdio_release_host(func);
		if (ret) {
			if (is_irq)
				mutex_unlock(&dnld_mutex);
			pr_info("%s can't write %d data into card %d\n", __func__, tx_len, ret);
			goto exit;
		}
		offset += tx_len;
	} while (total_len != offset);
	if (is_irq)
		mutex_unlock(&dnld_mutex);
	/*step 4 check fw_crc status*/
	pr_info("%s CRC_SUCCESS:0x%x\n", __func__, CRC_SUCCESS);
	if (check_scratch_status(func, CRC_SUCCESS) < 0)
		goto retry;

	pr_info("%s BOOT_SUCCESS:0x%x\n", __func__, BOOT_SUCCESS);
	/*step 5 check fw runing status*/
	if (check_scratch_status(func, BOOT_SUCCESS) < 0)
		goto exit;

	return 0;
exit:
	return -1;
}

#define SDIO_LOOPBACK_DEBUG 1
#ifdef SDIO_LOOPBACK_DEBUG

#define loopback_log(fmt, arg...) \
	pr_info("%s[%d]: "fmt"\r\n", __FUNCTION__, __LINE__, ##arg)
#else
#define loopback_log(fmt, arg...) \
	do {	\
	} while (0)
#endif

void *single_buf = NULL;
u16 single_buf_size = 0;

void *multi_port_buffer[16];
u16 multi_port_size[16] = {0};

void *aggr_rd_buf = NULL;
void *aggr_wr_buf = NULL;
int aggr_rd_size = 0;
int aggr_wr_size = 0;
u16 aggr_port_size[16] = {0};
u16 aggr_port_size_print[16] = {0};

static int asr_sdio_readw(struct sdio_func *func, uint32_t addr, uint16_t *data)
{
    int ret = 0;
    uint16_t val_w;
    uint8_t  val_b;

#if 0
    sdio_claim_host(func);
    val_w = sdio_readw(func, addr, &ret);
    sdio_release_host(func);
    loopback_log("read word val: %d", val_w);
#endif

    sdio_claim_host(func);
    val_b = sdio_readb(func, addr, &ret);
    sdio_release_host(func);
    if (ret) {
        loopback_log("Read bitmap low failed");
        return -ENODEV;
    }
    *data = (val_b & 0xFF);

    sdio_claim_host(func);
    val_b = sdio_readb(func, addr+1, &ret);
    sdio_release_host(func);
    if (ret) {
        loopback_log("Read bitmap upper failed");
        return -ENODEV;
    }
    *data |= ((val_b & 0xFF) << 8);

    //loopback_log("read byte val: %d", *data);

    return 0;
}

/*
	read_write_single_port_test: single port read or write
	write: 0 means read; 1 means write
*/
static void trigger_slaver_do_rxtest(struct sdio_func *func)
{
    int ret = 0;
    uint8_t reg_val = 0;

#if 0
    // Write Host_Pwr_Up bit to trigger card do initialization, used for combo SDIO3.0
    reg_val = (1 << 1);
    sdio_claim_host(func);
    sdio_writeb(func, reg_val, H2C_INTEVENT, &ret);
    sdio_release_host(func);

    // FIXME: wait client init
    msleep(500);
#endif
    reg_val = (0x1 << 3);
    sdio_claim_host(func);
    sdio_writeb(func, reg_val, H2C_INTEVENT, &ret);
    sdio_release_host(func);
    pr_info("Send H2C_INTEVENT done, ret: %d\n", ret);

    return;
}

static int start_flag = 0;
static int read_write_single_port_test(struct sdio_func *func, bool write)
{
	int ioport = 0, ret = 0, i;
	uint32_t addr;
	u16 port_index = 0, bitmap;
	u8 reg_val;
	uint32_t bitmap_addr_l, bitmap_addr_u;

	/*step 0: get ioport*/
	ioport = get_ioport(func);
	if (ioport < 0) {
		loopback_log("get ioport failed");
		return -ENODEV;
	}
	//port address occpy [17:14]
	ioport &= (0x1e000);

	/*step 1: wait for card rdy now only using irq way*/
	/* Check card status by polling or interrupt mode */
	if (start_flag) {
    	if (is_irq == 1) {
    		if (write) {
    			if (0 == wait_for_completion_timeout(&dnld_completion, msecs_to_jiffies(1000))) {
                    pr_info("wait to write timeout 1s\n");
                    return -ETIMEDOUT;
                }
            }
    		else {//read
    			if (0 == wait_for_completion_timeout(&upld_completion, msecs_to_jiffies(1000))) {
                    pr_info("wait to read timeout 1s\n");
                    return -ETIMEDOUT;
                }
            }
    	} else if (is_irq == 2) {
    		loopback_log("Do not support irq 2 mode.");
    	} else {
    	    loopback_log("Polling SDIO card status.");
    		ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
    		if (ret) {
    			loopback_log("SDIO device not ready!\n");
    			return -ENODEV;
    		}
    	}
	} else {
	    start_flag = 1;
	    loopback_log("first Polling SDIO card status.");
		ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
		if (ret) {
			loopback_log("SDIO device not ready!\n");
			start_flag = 0;
			return -ENODEV;
		}
    }

	//read bitmap to get which port is for opeartion as single port test only one port is valid
	if (!write) {
        bitmap_addr_l = RD_BITMAP_L;
        bitmap_addr_u = RD_BITMAP_U;
	} else {
        bitmap_addr_l = WR_BITMAP_L;
        bitmap_addr_u = WR_BITMAP_U;
	}

    ret = asr_sdio_readw(func, bitmap_addr_l, &bitmap);
    if (ret) {
        loopback_log("read bit map failed.");
        return -ENODEV;
    }

	port_index = ffs(bitmap) - 1;
    loopback_log("bitmap: %#x, port: %d", bitmap, port_index);

	if (!write) {
    	// read buffer size
	    ret = asr_sdio_readw(func, RD_LEN_L(port_index), &single_buf_size);
        if (ret) {
            loopback_log("read bit map failed.");
            return -ENODEV;
        }

		if (single_buf_size == 0) {
			loopback_log("single port test card->host return buf size is 0, error!");
			return -ENODEV;
		}
		single_buf = kzalloc((size_t)single_buf_size, GFP_KERNEL);
		if (!single_buf) {
			loopback_log("alloc buffer failed, size: %d", single_buf_size);
			return -ENOMEM;
		}
	} else {
		if (!single_buf || single_buf_size == 0) {
			loopback_log("Write failed, buf is null or buf len is 0");
			return -ENODEV;
		}
	}

	/*step 2: do a read or write operation*/
	addr = ioport | (port_index);
	loopback_log("CMD53 %s, addr:0x%x port:%d (bitmap:%#x) buf len:%d (from rd len register[%d]:0x%x)\n",
    			write ? "W ->" : "R <-", addr,
    			port_index, bitmap, single_buf_size,
    			port_index, RD_LEN_L(port_index));

	sdio_claim_host(func);
	ret = (write ? sdio_writesb(func, addr, single_buf, (int)single_buf_size) : sdio_readsb(func, single_buf, addr, (int)single_buf_size));
	sdio_release_host(func);

	if (ret) {
		loopback_log("%s data failed\n", write ? "write" : "read");
		goto err;
	}

	loopback_log("%s Done, data: ", write?"W":"R");

    if (!write)
    {
#if 0
    	print_hex_dump(KERN_DEBUG, "RX: ", DUMP_PREFIX_OFFSET, 16, 1,
    		        single_buf, single_buf_size, true);
        //*((uint8_t*)single_buf) = 1;
#endif
    }

err:
	//free buffer after write cycle
	if (write) {
		kfree(single_buf);
		single_buf = NULL;
		single_buf_size = 0;
	}

	return ret;
}

/*
	single_port_tests:
	return 0 as success, return < 0 as fail;
*/
static int single_port_tests(struct sdio_func *func)
{
    // same in device side
	int block_size_arr[] = {16, 32, 256};
	int loop, test_times, ret;

	reinit_completion(&upld_completion);
	reinit_completion(&dnld_completion);

	for (loop = 0; loop < ARRAY_SIZE(block_size_arr); loop++) {

		sdio_claim_host(func);
		ret = sdio_set_block_size(func, block_size_arr[loop]);
		sdio_release_host(func);
		if (ret) {
			loopback_log("Set block size failed");
			return ret;
		}
		loopback_log("Start single port test...");

		// loop test for each port
		for (test_times = 0; test_times < 16; test_times++) {
		    loopback_log("single port test: block size: %d, cycle: %d\r\n", block_size_arr[loop], test_times);

			ret = read_write_single_port_test(func, 0);
			if (ret) {
				loopback_log("read single port test failed");
				return ret;
			}
			ret = read_write_single_port_test(func, 1);
			if (ret) {
				loopback_log("write single port test failed");
				return ret;
			}
		}

		loopback_log("*******************************************\r\n"
		             "*********** signle port test done! ********\r\n"
		             "*******************************************\r\n");
	}

	return 0;
}

static int read_write_multi_port_test(struct sdio_func *func, bool write)
{
	int ioport, ret, i, j, port_used;
	uint32_t addr;
	u16 port_size;
	u16 bitmap;
	u8 reg_val;
	uint32_t bitmap_addr_l, bitmap_addr_u;
	u16 port_index = 0;

	/*step 0: get ioport*/
	ioport = get_ioport(func);
	if (ioport < 0) {
		loopback_log("get ioport failed\n");
		return -ENODEV;
	}
	ioport &= (0x1e000);

	/*step 1: wait for card rdy*/
	if (is_irq == 1) {
		if (write)
			wait_for_completion(&dnld_completion);
		else//read
			wait_for_completion(&upld_completion);
	} else if (is_irq == 2) {
		loopback_log("can't support now");
	} else {

        loopback_log("Polling SDIO card status.");
		ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
		if (ret) {
			loopback_log("card is not rdy!");
			return -ENODEV;
		}
	}

	//read bitmap to know which ports are for opeartion
	// get available ports bitmap for data transmate
	if (!write) {
        bitmap_addr_l = RD_BITMAP_L;
        bitmap_addr_u = RD_BITMAP_U;
	} else {
        bitmap_addr_l = WR_BITMAP_L;
        bitmap_addr_u = WR_BITMAP_U;
	}

    ret = asr_sdio_readw(func, bitmap_addr_l, &bitmap);
    if (ret) {
        loopback_log("read bit map failed.");
        return -ENODEV;
    }

	port_index = ffs(bitmap) - 1;
    loopback_log("bitmap: %#x, port: %d", bitmap, port_index);

	port_used = 0;

	//do multi read/write operation
	// need make sure read before write
	for (i = 0; i < 16; i++) {
	    //in test phase rd_bitmap can not match the wr_bitmap, but from right to left to assign the buffer to write

	    // find each free port
		if (bitmap & (1 << i)) {

			if (write) {//write

				addr = ioport | i;
			 	port_size  = multi_port_size[port_used];

				sdio_claim_host(func);
				ret = sdio_writesb(func, addr, multi_port_buffer[port_used], port_size);
				sdio_release_host(func);
				if (ret) {
					loopback_log("%s data failed\n", "write");
					return ret;
				}

				//free buffer
				kfree(multi_port_buffer[port_used]);
				multi_port_buffer[port_used] = NULL;
				multi_port_size[port_used] = 0;
				port_used++;
			} else {//read

			    ret = asr_sdio_readw(func, RD_LEN_L(i), &port_size);
				if (ret) {
					loopback_log(" multi port read RD_LEN_L failed\n");
					return -ENODEV;
				}

				if (port_size == 0) {
					loopback_log("read port(%d) len is 0, error!", i);
					return -ENODEV;
				}

				//alloc buffer
				multi_port_buffer[port_used] = kzalloc((size_t)port_size, GFP_KERNEL);
				multi_port_size[port_used] = port_size;
				if (!multi_port_buffer[port_used]) {
					for (j = 0; j < port_used; j++) {
						if (multi_port_buffer[port_used])
							kfree(multi_port_buffer[port_used]);
					}
					loopback_log("alloc memory failed during multi-port test!, cycle: %d", i);
					return -ENOMEM;
				}

				addr = ioport | i;

				sdio_claim_host(func);
				ret = sdio_readsb(func, multi_port_buffer[port_used], addr, port_size);
				sdio_release_host(func);
				if (ret) {
					loopback_log("Read data failed, cycle: %d", i);
					return ret;
				}
				port_used++;
			}

            loopback_log(" %s port:%d(from bitmap:0x%x) addr:%d buf_len:%d (from rd len [%d]:0x%x) port_used:%d",
                    write?"W-> ":"R<- ", i, bitmap, addr, port_size, i, RD_LEN_L(i), port_used);
		}
	}

	return 0;
}

static int multi_port_tests(struct sdio_func *func)
{
	int loop, test_times;
	int block_size_arr[] = {16, 32, 256};
	int ret;

	for (loop = 0; loop < 16; loop++)
		multi_port_buffer[loop] = NULL;

	reinit_completion(&upld_completion);
	reinit_completion(&dnld_completion);

	// loop 3 different block size with each port
	for (loop = 0; loop < ARRAY_SIZE(block_size_arr); loop++) {

		sdio_claim_host(func);
		ret = sdio_set_block_size(func, block_size_arr[loop]);
		sdio_release_host(func);
		if (ret) {
			loopback_log("%s set block size failed\n", __func__);
			return ret;
		}
		loopback_log("set block_size:%d", block_size_arr[loop]);

		for (test_times = 0; test_times < 16; test_times++) {
			ret = read_write_multi_port_test(func, 0);
			if (ret) {
				loopback_log("%s read single port test failed\n", __func__);
				return ret;
			}
			ret = read_write_multi_port_test(func, 1);
			if (ret) {
				loopback_log("%s write single port test failed\n", __func__);
				return ret;
			}
		}
	}

	return 0;
}

static int read_write_agg_port_test(struct sdio_func *func, int block_size, bool write)
{
	int ioport, ret;
	uint32_t addr;
	int agg_bitmap, i, agg_num;
	int start, end, port_used;
	u16 port_size, bitmap;
	u16 start_port;
	u8 reg_val;
	uint32_t bitmap_addr_l, bitmap_addr_u;

	/*step 0: get ioport*/
	ioport = get_ioport(func);
	if (ioport < 0) {
		loopback_log("%s get ioport failed\n", __func__);
		return -EINVAL;
	}
	ioport &= (0x1e000);

	/*step 1: wait for card rdy*/
	if (is_irq == 1) {
		if (write) {
			loopback_log("write_completion start\n");
			wait_for_completion(&dnld_completion);
			loopback_log("write_completion end\n");
		} else {//read
			loopback_log("read_completion start\n");
			wait_for_completion(&upld_completion);
			loopback_log("read_completiion end\n");
		}
	} else if (is_irq == 2) {
		loopback_log("can't support now");
	} else {
		ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
		if (ret) {
			loopback_log("card is not rdy!");
			return -ENODEV;
		}
	}

	//read bitmap to know which ports are for opeartion
	// get available ports bitmap for data transmate
	if (!write) {
        bitmap_addr_l = RD_BITMAP_L;
	} else {
        bitmap_addr_l = WR_BITMAP_L;
	}

    ret = asr_sdio_readw(func, bitmap_addr_l, &bitmap);
    if (ret) {
        loopback_log("read bit map failed.");
        return -ENODEV;
    }

	if (!write) {
		aggr_rd_size = 0;
		port_used = 0;
		for (i = 0; i < 16; i++) {
			aggr_port_size[i] = 0;
			aggr_port_size_print[i] = 0;
		}
		loopback_log("aggr_rd_size = 0");

		for (i = 0; i < 16; i++) {
			if (bitmap & (1 << i)) {
			    ret = asr_sdio_readw(func, RD_LEN_L(i), &port_size);
				if (ret || (0 == port_size)) {
					loopback_log(" aggr port read RD_LEN failed\n");
					return -ENODEV;
				}

				loopback_log(" + %d(from [%d]:0x%x 0x%x)", port_size, i, RD_LEN_L(i), RD_LEN_U(i));

                // record each port size
				aggr_port_size[port_used] = port_size;
				aggr_port_size_print[i] = port_size;
				port_used++;
				aggr_rd_size += port_size;
			}
		}

		//add 4 fake size to store mark char
		aggr_rd_size += 4;
		loopback_log(";\n");
		aggr_rd_buf = kzalloc((size_t)aggr_rd_size, GFP_KERNEL);
		if (!aggr_rd_buf) {
			loopback_log(" can't allock buffer for aggregation test, length: %d", aggr_rd_size);
			return -ENOMEM;
		}
	} else {//write
		//[2byte length][data][padding] should in block size
		aggr_wr_size = 0;
		port_used = 0;
		loopback_log("aggr_wr_size = 0");
		for (i = 0; i < 16; i++) {
			if (bitmap & (1 << i)) {
				aggr_wr_size += aggr_port_size[port_used];//data size
				loopback_log(" + %d", aggr_port_size[port_used]);
				// aligned with block size
				if ((aggr_port_size[port_used]) % block_size) {
					aggr_wr_size += (block_size - ((aggr_port_size[port_used]) % block_size));
					loopback_log(" + %d", (block_size - ((aggr_port_size[port_used]) % block_size)));
				}
				port_used++;
			}
		}
		loopback_log(";\n");

	}

	//found the bitmap area of aggr mode
	start = ffs(bitmap);
	end = fls(bitmap);
	//assume that there is full '1' between ffs and fls, take no care other situations now
	/*
		1st situation

				 >fls - 1<   >ffs - 1<
					  |           |
		--------------------------------
		|             |///////////|    |
		|             |///////////|    |
		|             |///////////|    |
		--------------------------------
		              ^           ^
					  |           |
					 end		 start
		2nd situation

   >fls - 1<					  >ffs - 1<
	    |                              |
		--------------------------------
		|/////|                    |///|
		|/////|       			   |///|
		|/////|                    |///|
		--------------------------------
		      ^                    ^
			  |                    |
			 start                end
	*/
	//2nd situation
	if (end - start == 15) {
		int zero_start, zero_end;
		zero_start = ffs(~bitmap & 0xFFFF);
		zero_end = fls(~bitmap & 0xFFFF);
		start_port = zero_end;
		agg_num = (16 - (zero_end - zero_start + 1));
		agg_bitmap = ((1 << agg_num) - 1) & 0xFF;
		addr = ioport | (1 << 12) | (agg_bitmap << 4) | start_port;
	} else {//1st situation
		start_port = start - 1;
		agg_num = (end - start + 1);
		agg_bitmap = ((1 << agg_num) - 1) & 0xFF;
		addr = ioport | (1 << 12) | (agg_bitmap << 4) | start_port;
	}

	if (write && aggr_wr_size == 192) {
		loopback_log("%s do a joke in write change size from %d to", __func__, aggr_wr_size);
		aggr_wr_size = 72;
		loopback_log("%d\n", aggr_wr_size);
	}

	/*step 3: do read/write operation*/
	loopback_log("%s start port:%d (from bitmap:0x%x) port count:%d agg_bitmap:0x%x addr:0x%x buf_size:%d\n",
					write ? "W->" : "R<-", start_port, bitmap, agg_num, agg_bitmap, addr, write ? aggr_wr_size : aggr_rd_size - 4);
	sdio_claim_host(func);

	ret = (write ? sdio_writesb(func, addr, aggr_rd_buf, aggr_wr_size) : sdio_readsb(func, aggr_rd_buf, addr, aggr_rd_size - 4));
	sdio_release_host(func);
	if (ret) {
		loopback_log("%s %s data failed\n", __func__, write ? "write" : "read");
		return ret;
	}
	if (write) {
		u32 *ptemp;
		if (is_loopback_display) {
			int i, temp_port;
			ptemp = (u32*)aggr_rd_buf;
			for (i = 0; i < agg_num; i++) {
				temp_port = (start_port + i) % 16;
				loopback_log("%s write port[%d] with size:%d: first:0x%x - last:0x%x\n",
						__func__, temp_port, aggr_port_size_print[temp_port], *ptemp, *(ptemp + aggr_port_size_print[temp_port] / 4 - 1));
				ptemp += (aggr_port_size_print[temp_port] / 4);
			}
		}
		kfree(aggr_rd_buf);
		//kfree(aggr_wr_buf);
		aggr_wr_size = 0;
	} else {
		u32 *ptemp;
		//add fake 0x12345678 at the rx_buf's tail
		ptemp = ((u32*)aggr_rd_buf) + (aggr_rd_size / 4 - 1);
		*ptemp = 0x12345678;

		if (is_loopback_display) {
			int i, temp_port;
			ptemp = (u32*)aggr_rd_buf;
			for (i = 0; i < agg_num; i++) {
				temp_port = (start_port + i) % 16;
				loopback_log("%s read port[%d] with size:%d: first:0x%x - last:0x%x\n",
						__func__, temp_port, aggr_port_size_print[temp_port], *ptemp, *(ptemp + aggr_port_size_print[temp_port] / 4 - 1));
				ptemp += (aggr_port_size_print[temp_port] / 4);
			}

		}
	}
	return ret;
}

/*
	aggr_port_test: do aggr mode test
*/
static int aggr_port_tests(struct sdio_func *func)
{
	int block_size_arr[] = {16, 32, 256};
	int loop, test_times, ret;
	int aggr_block_size;

	reinit_completion(&upld_completion);
	reinit_completion(&dnld_completion);

	for (loop = 0; loop < ARRAY_SIZE(block_size_arr); loop++) {
		sdio_claim_host(func);
		if (direct_block_size)
			aggr_block_size = direct_block_size;
		else
			aggr_block_size = block_size_arr[loop];
		ret = sdio_set_block_size(func, aggr_block_size);
		sdio_release_host(func);
		if (ret) {
			loopback_log("set block size[%d] failed", aggr_block_size);
			return ret;
		}

		loopback_log("set block_size: %d", aggr_block_size);
		for (test_times = 0; test_times < 16; test_times++) {
			ret = read_write_agg_port_test(func, aggr_block_size, 0);
			if (ret) {
				loopback_log("aggr mode read failed");
				return ret;
			}
			ret = read_write_agg_port_test(func, aggr_block_size, 1);
			if (ret) {
				loopback_log("aggr mode write failed");
				return ret;
			}

			loopback_log("cycle: %d\r\n", test_times);
		}
	}

	return 0;
}

/*
	sdio_loopback_test: execute loopback test case
*/
static int sdio_loopback_test(struct sdio_func *func)
{
	int ret;
	int i;

	trigger_slaver_do_rxtest(func);

	i = 0;
	while (i < loopback_times) {
		pr_info("####################### loopback_times: %d, i %d\n", loopback_times, i);
		/*single port test*/
		if (loopback_test_mode & 0x1) {
			ret = single_port_tests(func);
			if (ret < 0)
				return ret;
		}
		/*multi port test*/
		if (loopback_test_mode & 0x2) {
			ret = multi_port_tests(func);
			if (ret < 0)
				return ret;
		}
		/*aggregation mode test*/
		if (loopback_test_mode & 0x4) {
			ret = aggr_port_tests(func);
			if (ret < 0)
				return ret;
		}
		i++;
	}
	return 0;
}

/*
	asr_sdio_function_irq: isr in data1 if exist
*/
static void asr_sdio_function_irq(struct sdio_func *func)
{
#if 0
	uint8_t reg_val;
	int ret = -1;

	gpio_irq_times++;
	pr_info("%s gpio_irq_times:%d enter ...\n", __func__, gpio_irq_times);
	reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
	if (ret)
		return;
	if (reg_val & HOST_INT_DNLD_ST) {
		mutex_lock(&dnld_mutex);
		reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
		reg_val &= ~HOST_INT_DNLD_ST;

		sdio_writeb(func, reg_val, HOST_INT_STATUS, &ret);
		complete(&dnld_completion);
		pr_info("%s DNLD STATUS processed\n", __func__);
		mutex_unlock(&dnld_mutex);
	}
	pr_info("%s done\n", __func__);
#else
	uint8_t reg_val;
	int ret = -1;
	//pr_info("%s\r\n", __func__);
	gpio_irq_times++;
	sdio_claim_host(func);
	reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
	sdio_release_host(func);
	if (ret)
		return;
	//pr_info("%s gpio_irq_times:%d enter ... int status:0x%x\n", __func__, gpio_irq_times, reg_val);
	if (reg_val & HOST_INT_UPLD_ST) {
		mutex_lock(&upld_mutex);
		sdio_claim_host(func);
		reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
		sdio_release_host(func);
		reg_val &= ~HOST_INT_UPLD_ST;

		sdio_claim_host(func);
		sdio_writeb(func, reg_val, HOST_INT_STATUS, &ret);
		sdio_release_host(func);
		complete(&upld_completion);
		pr_info("%s UPLD STATUS processed\n", __func__);
		mutex_unlock(&upld_mutex);
	}
	if (reg_val & HOST_INT_DNLD_ST) {
		mutex_lock(&dnld_mutex);
		sdio_claim_host(func);
		reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
		sdio_release_host(func);
		reg_val &= ~HOST_INT_DNLD_ST;

		sdio_claim_host(func);
		sdio_writeb(func, reg_val, HOST_INT_STATUS, &ret);
		sdio_release_host(func);
		complete(&dnld_completion);
		pr_info("%s DNLD STATUS processed\n", __func__);
		mutex_unlock(&dnld_mutex);
	}
	//pr_info("%s done\n", __func__);

#endif
	return;
}

static void asr_sdio_trigger_fw_init(struct sdio_func *func)
{
    int ret = 0;
    uint8_t reg_val = 0;

    // Write Host_Pwr_Up bit to trigger card do initialization, used for combo SDIO3.0
    reg_val = (1 << 1);
    sdio_claim_host(func);
    sdio_writeb(func, reg_val, H2C_INTEVENT, &ret);
    sdio_release_host(func);

    msleep(500);

    pr_info("Send H2C_INTEVENT.Host_Pwr_Up done, ret: %d\n", ret);

    return;
}

#if 0
static irqreturn_t asr_sdio_gpio_irq_thread(int irq, void *dev)
{
	struct sdio_func *func = (struct sdio_func *)(dev);
	uint8_t reg_val;
	int ret = -1;
	pr_info("%s\r\n", __func__);
	gpio_irq_times++;
	sdio_claim_host(func);
	reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
	sdio_release_host(func);
	if (ret)
		return IRQ_HANDLED;
	pr_info("%s gpio_irq_times:%d enter ... int status:0x%x\n", __func__, gpio_irq_times, reg_val);
	if (reg_val & HOST_INT_UPLD_ST) {
		mutex_lock(&upld_mutex);
		sdio_claim_host(func);
		reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
		sdio_release_host(func);
		reg_val &= ~HOST_INT_UPLD_ST;

		sdio_claim_host(func);
		sdio_writeb(func, reg_val, HOST_INT_STATUS, &ret);
		sdio_release_host(func);
		complete(&upld_completion);
		pr_info("%s UPLD STATUS processed\n", __func__);
		mutex_unlock(&upld_mutex);
	}
	if (reg_val & HOST_INT_DNLD_ST) {
		mutex_lock(&dnld_mutex);
		sdio_claim_host(func);
		reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
		sdio_release_host(func);
		reg_val &= ~HOST_INT_DNLD_ST;

		sdio_claim_host(func);
		sdio_writeb(func, reg_val, HOST_INT_STATUS, &ret);
		sdio_release_host(func);
		complete(&dnld_completion);
		pr_info("%s DNLD STATUS processed\n", __func__);
		mutex_unlock(&dnld_mutex);
	}
	pr_info("%s done\n", __func__);

	return IRQ_HANDLED;
}
#endif

static int asr_sdio_get_func_num(struct sdio_func *func)
{
	int ret;
	u8 reg_val;

	sdio_claim_host(func);
	reg_val = sdio_readb(func, SDIO_CCCR_CCCR, &ret);
	sdio_release_host(func);
	if (ret)
		return ret;
	reg_val |= 0x02;
	sdio_claim_host(func);
	sdio_writeb(func, reg_val, SDIO_CCCR_CCCR, &ret);
	sdio_release_host(func);
	udelay(20);
	//if (ret)
	//	return ret;

	//sdio_claim_host(func);
	//ret = sdio_cmd5(func);
	//sdio_release_host(func);
	//udelay(20);
	return ret;
}

static int asr_sdio_loopback_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	const struct firmware *fw_img = NULL;
	int ret = 0;
	int i = 0;


	asr_sdio_trigger_fw_init(func);

	asr_sdio_get_func_num(func);

	if(func_number)
		func->num = func_number;
	sdio_claim_host(func);
	/*enable func*/
	ret = sdio_enable_func(func);
	if (0 != ret)
	{
        pr_info("%s failed, ret: %d\n", __FUNCTION__, ret);
        return ret;
	}

	//asr_sdio_host_clk_en(true);

    pr_info("SDIO card num: %d, func num: %d\n", func->card->sdio_funcs, func->num);
	for (i = 0; i < func->card->num_info; i++)
	{
    	pr_info("SDIO info: %s\n", func->card->info[i]);

	}

	pr_info("class = %#X, vendor = %#X, device = %#X\n",
	        func->class, func->vendor, func->device );

	/*set block size 256*/
	ret = sdio_set_block_size(func, 2 * SDIO_BLOCK_SIZE);
	g_block_size = 2 * SDIO_BLOCK_SIZE;
	/*set sdio function irq handler*/
	sdio_release_host(func);
	/*every transfer will compare with func->cur_blksz, please check sdio_io_rw_ext_helper, it will decide
	which size use block mode and which size using byte mode by check:
	(size > sdio_max_byte_size(func))*/
	func->card->quirks |= MMC_QUIRK_BLKSZ_FOR_BYTE_MODE;
#if 0
	/*enable host interrupt:
		is_irq = 0 no interrupts using polling way
		is_irq = 1 using gpio interrupt
		is_irq = 2 using data1 line as interrupt source
	*/
	if (is_irq == 1) {
		mutex_init(&dnld_mutex);
		mutex_init(&upld_mutex);
		asr_sdio_enable_int(func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);
		ret = request_threaded_irq(card_irq, NULL, asr_sdio_gpio_irq_thread,
					/*IRQF_SHARED | */IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "card irq", func);
		pr_info("%s host in gpio-irq way...\n", __func__);
		/*irq way + tasklet*/
		//ret = request_irq(card_irq, asr_sdio_gpio_irq, IRQF_TRIGGER_FALLING, "card irq", func);
	} else if (is_irq == 2) {
		asr_sdio_enable_int(func, HOST_INT_DNLD_EN);
		sdio_claim_host(func);
		sdio_claim_irq(func, asr_sdio_function_irq);
		sdio_release_host(func);
		pr_info("%s host in data1-irq way...\n", __func__);
	} else
		pr_info("%s host in polling way...\n", __func__);
#endif
	mutex_init(&dnld_mutex);
	mutex_init(&upld_mutex);
#if 1 //ZLL
	sdio_claim_host(func);
	ret = sdio_claim_irq(func, asr_sdio_function_irq);
	sdio_release_host(func);
	asr_sdio_enable_int(func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);
#endif
	/*request firmware*/
	if (fw_name) {
		pr_info("%s request firmware:%s\n", __func__, fw_name);
		ret = request_firmware(&fw_img, fw_name, &func->dev);
		if (ret) {
			pr_info("%s request firmware failed ret:%d\n", __func__, ret);
			goto exit;
		}
		dev_set_drvdata(&func->dev, (void *)fw_img);
		pr_info("%s fw_len:%d \n ", __func__, fw_img->size);
		if (download_firmware(func, fw_img) < 0)
			goto release_fw;
	}

	/*after download firmware ready, run loopback test*/
	if (is_loopback_test) {
		ret = sdio_loopback_test(func);
		if (ret)
			goto release_fw;
	}
	pr_info("%s done\n", __func__);

	return 0;
release_fw:
	if (fw_name)
		release_firmware(fw_img);
exit:
	/*FIXME in test comment it */
	if (is_irq == 1) {
		sdio_claim_host(func);
		sdio_release_irq(func);
		sdio_release_host(func);
		;//free_irq(card_irq, func);
    }
	else if (is_irq == 2) {
		;
		/*
		sdio_claim_host(func);
		sdio_release_irq(func);
		sdio_release_host(func);
		*/
	}
	return -1;
}

static void asr_sdio_loopback_remove(struct sdio_func *func)
{
	struct firmware *fw_img;

	sdio_claim_host(func);
	sdio_disable_func(func);
	sdio_release_irq(func);
	sdio_release_host(func);
	if (fw_name) {
		fw_img = (struct firmware *)dev_get_drvdata(&func->dev);
		release_firmware(fw_img);
	}
	if (is_irq == 1)
		free_irq(card_irq, func);
	else if (is_irq == 2)
		sdio_release_irq(func);
	pr_info("%s do nothing\n", __func__);
}

static const struct sdio_device_id asr_sdio_loopback_ids[] = {
    // combo/lega
    {SDIO_DEVICE(0x424c, 0x6006)},
	{SDIO_DEVICE(0x02df, 0x912d)},
    // canon
	{SDIO_DEVICE(0x424c, 0x700a)},
	//{SDIO_DEVICE(0x424c, 0x700b)},
	{},
};

MODULE_DEVICE_TABLE(sdio, asr_sdio_loopback_ids);
int asr_sdio_loopback_suspend(struct device *dev)
{
	return 0;
}

int asr_sdio_loopback_resume(struct device *dev)
{
	return 0;
}

const struct dev_pm_ops asr_sdio_loopback_pm_ops = {
	.suspend = asr_sdio_loopback_suspend,
	.resume = asr_sdio_loopback_resume,
};

struct sdio_driver asr_sdio_loopback_driver = {
	.name = "asr_sdio_loopback",
	.drv = {
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &asr_sdio_loopback_pm_ops,
#endif
	},
	.id_table = asr_sdio_loopback_ids,
	.probe = asr_sdio_loopback_probe,
	.remove = asr_sdio_loopback_remove,
};
static const struct of_device_id asr_sdio_of_match[] = {
	{
		.compatible = "asr,asr-platform",
	},
	{},
};

MODULE_DEVICE_TABLE(of, asr_sdio_of_match);

#if 1
int asr_sdio_loopback_register_drv(void)
{
    int ret = 0;
    ret = sdio_register_driver(&asr_sdio_loopback_driver);
    return ret;
}

static int asr_sdio_get_sdio_host(struct device *dev)
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

	g_asr_sdio_para.mmc = host->mmc;

	dev_err(dev, "%s: get sdio host(%s) success\n", __func__, mmc_hostname(g_asr_sdio_para.mmc));

	return 0;
}

static void asr_sdio_host_clk_en(bool clk_en)
{
//#if 1
#ifdef ASR_MACH_PXA1826_CLK_EN
	struct platform_device *pdev;
	struct sdhci_pxa_platdata *sdhci_pdata;

	pdev = to_platform_device(mmc_dev(g_asr_sdio_para.mmc));
	sdhci_pdata = pdev->dev.platform_data;

	if (clk_en) {
		//fix clock issue for sdio rx
		clk_prepare_enable(sdhci_pdata->clk);
	} else {
		clk_disable_unprepare(sdhci_pdata->clk);
	}
	loopback_log("clk=%lu clk_en=%d", sdhci_pdata->clk->rate, clk_en);
#endif
}

void asr_sdio_detect_change(void)
{

	if (!g_asr_sdio_para.mmc) {
		dev_err(g_asr_sdio_para.dev, "%s: fail\n", __func__);
		return;
	}

	dev_err(g_asr_sdio_para.dev, "%s: sdio host(%s)\n", __func__, mmc_hostname(g_asr_sdio_para.mmc));

	mmc_detect_change(g_asr_sdio_para.mmc, 0);

	return;
}

int asr_sdio_platform_power_off(struct device *dev)
{
#ifdef CONFIG_ASR_SDIO
	/* do sdio detect */
	asr_sdio_host_clk_en(false);
	asr_sdio_detect_change();
#endif

	return 0;
}

static int asr_sdio_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	g_asr_sdio_para.dev_driver_remove = false;
	g_asr_sdio_para.dev = &pdev->dev;

	dev_info(&pdev->dev, "%s: start\n", __func__);

    // get mmc host
	ret = asr_sdio_get_sdio_host(&pdev->dev);
	if (ret) {
		return ret;
	}

	asr_sdio_host_clk_en(true);
	asr_sdio_detect_change();

#ifdef CONFIG_ASR_SDIO_LOOPBACK
    ret = asr_sdio_loopback_register_drv();
    if (ret) {
		dev_err(&pdev->dev, "%s: asr_sdio_loopback_register_drv fail\n", __func__);
		asr_sdio_platform_power_off(&pdev->dev);
		return ret;
	}
#endif
	return 0;
}

static int asr_sdio_platform_remove(struct platform_device *pdev)
{
	void *data;

	g_asr_sdio_para.dev_driver_remove = true;

	data = platform_get_drvdata(pdev);
	if (data == NULL)
		return -ENODEV;
	dev_info(&pdev->dev, "%s: start\n", __func__);

	asr_sdio_platform_power_off(&pdev->dev);

	return 0;
}

static struct platform_driver asr_sdio_platform_driver = {
	.probe = asr_sdio_platform_probe,
	.remove = asr_sdio_platform_remove,
	.driver = {
		   .name = "asr-sdio-platform",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = asr_sdio_of_match,
#endif

		   },
};
#endif

#if 1
static int __init asr_sdio_init(void)
{
	pr_info("%s enter...\n", __func__);
	platform_driver_register(&asr_sdio_platform_driver);
	sdio_register_driver(&asr_sdio_loopback_driver);
	pr_info("%s leave\n", __func__);
	return 0;
}

static void __exit asr_sdio_exit(void)
{
	pr_info("%s enter...\n", __func__);
	sdio_unregister_driver(&asr_sdio_loopback_driver);
    pr_info("sdio unregister done\r\n");
	platform_driver_unregister(&asr_sdio_platform_driver);
	pr_info("%s leave\n", __func__);
}
module_init(asr_sdio_init);
module_exit(asr_sdio_exit);
#endif
module_param(fw_name, charp, 0660);
module_param(is_irq, int, 0660);
module_param(loopback_times, int, 0660);
module_param(loopback_test_mode, int, 0660);
module_param(is_loopback_test, bool, 0660);
module_param(func_number, int, 0660);
module_param(is_loopback_display, bool, 0660);
module_param(direct_block_size, int, 0660);
module_param(direct_pass, bool, 0660);
module_param(crc_value, int, 0660);
module_param(is_host_int, int, 0660);
module_param(test_irq_count, int, 0660);

MODULE_PARM_DESC(fw_name, "Specify the firmware name");
MODULE_AUTHOR("qipanli@asrmicro.com");
MODULE_DESCRIPTION("asr sdio sdio driver");
MODULE_LICENSE("GPL");
