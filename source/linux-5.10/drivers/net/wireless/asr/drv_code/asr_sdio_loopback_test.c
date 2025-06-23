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
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/card.h>
#include "asr_platform.h"
#include "asr_defs.h"

#define SDIO_LOOPBACK_DEBUG 1
#ifdef SDIO_LOOPBACK_DEBUG
#define loopback_log(fmt, arg...) \
	pr_info("%s[%d]: "fmt"\r\n", __FUNCTION__, __LINE__, ##arg)
#else
#define loopback_log(fmt, arg...) \
	do {	\
	} while (0)
#endif

#define H2C_INTEVENT                0x0
#define HOST_INT_RSR                0x1
#define HOST_INTMASK                0x2
#define HOST_INT_UPLD_EN            (1 << 0)
#define HOST_INT_DNLD_EN            (1 << 1)
#define HOST_INT_UNDER_FLOW_EN      (1 << 2)
#define HOST_INT_OVER_FLOW_EN       (1 << 3)
#define HOST_INT_MASK               (HOST_INT_UPLD_EN | HOST_INT_DNLD_EN | HOST_INT_UNDER_FLOW_EN | HOST_INT_OVER_FLOW_EN)

#define HOST_INT_STATUS             0x3
#define HOST_INT_UPLD_ST            (1 << 0)
#define HOST_INT_DNLD_ST            (1 << 1)
#define HOST_INT_UNDER_FLOW_ST      (1 << 2)
#define HOST_INT_OVER_FLOW_ST       (1 << 3)

#define RD_BITMAP_L                 0x4
#define RD_BITMAP_U                 0x5
#define WR_BITMAP_L                 0x6
#define WR_BITMAP_U                 0x7

#define RD_LEN_L(index)             (0x8 + (index) * 2)
#define RD_LEN_U(index)             (0x9 + (index) * 2)

#define C2H_INTEVENT                0x30
#define C2H_DNLD_CARD_RDY           (1 << 0)
#define C2H_UPLD_CARD_RDY           (1 << 1)
#define C2H_CIS_CARD_RDY            (1 << 2)
#define C2H_IO_RDY                  (1 << 3)

#define SCRATCH_0                   0x60
#define SCRATCH_1                   0x61
#define CRC_SUCCESS                 (1 << 14)
#define CRC_FAIL                    0xABCE
#define BOOT_SUCCESS                (1 << 15)

#define SCRATCH0_2                  0x62
#define SCRATCH0_3                  0x63
#define SCRATCH1_0                  0x64
#define SCRATCH1_1                  0x65
#define SCRATCH1_2                  0x66
#define SCRATCH1_3                  0x67

#define IO_PORT_0                   0x78
#define IO_PORT_1                   0x79
#define IO_PORT_2                   0x7a

#define MAX_POLL_TRIES              100
#define SDIO_BLOCK_SIZE_DLD         512
#define ASR_ALIGN_DLDBLKSZ_HI(x)    (((x) + SDIO_BLOCK_SIZE_DLD - 1) & (~(SDIO_BLOCK_SIZE_DLD - 1)))

#define ALIGN_ADDR(p, a)            (void *)(((uintptr_t)(p) + (a) - 1) & ~((a) - 1))
#define DMA_ALIGNMENT               32

#define SDIO_REG_READ_LENGTH        64

#define THROUGHPUT_TEST_CNT         40000

struct sdio_host_sec_fw_header {
	uint32_t sec_fw_len;	// total length of FW: should be 4 byte align
	uint32_t sec_crc;	// fw bin crc32 results
	uint32_t chip_ram_addr;	// address where fw be loaded to
	uint32_t transfer_unit;	// download length in once cmd53 transmition
	uint32_t transfer_times;	// TX times needed for download FW, not include the first cmd transmition
};

struct sdio_fw_sec_fw_header {
	uint32_t sec_fw_len;    // total length of FW: should be 4 byte align
	uint32_t sec_crc;       // fw bin crc32 results
	uint32_t chip_ram_addr; // address where fw be loaded to
};

static int ioport;
static char *fw_name = NULL;
static int is_irq = 0;
static int loopback_times = 1;
static int loopback_test_mode = 0x7;
static bool is_loopback_test = 0;
static int func_number = 0;
static bool is_loopback_display = 0;
static int irq_times;
static int start_flag = 0;
static bool loopback_rw_sg = 0;

void *single_buf = NULL;
u16 single_buf_size = 0;

void *multi_port_buffer[16];
u16 multi_port_size[16] = {0};

void *aggr_rd_buf = NULL;
int aggr_rd_size = 0;
int aggr_wr_size = 0;
u16 aggr_port_size[16] = {0};
u16 aggr_port_size_print[16] = {0};
u8 *aggr_port_buf[16];

static bool is_throughput_test = 0;
static int throughput_block_size = 512;
static int throughput_packet_size = 2048;
static int throughput_cnt = 0;
void *throughput_buf = NULL;
static u32 start_jiffies = 0;
static u32 stop_jiffies = 0;
static u8 *sdio_regs = NULL;

DECLARE_COMPLETION(dnld_completion);
DECLARE_COMPLETION(upld_completion);
static DEFINE_MUTEX(dnld_mutex);
static DEFINE_MUTEX(upld_mutex);

/*
	get_ioport: get ioport value
*/
static int sdio_get_ioport(struct sdio_func *func)
{
	int ret;
	u8 reg;
	int ioport;

	/* get out ioport value */
	sdio_claim_host(func);
	reg = sdio_readb(func, IO_PORT_0, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s can't read 0x%x\n", __func__, IO_PORT_0);
		goto exit;
	}
	ioport = reg & 0xff;

	sdio_claim_host(func);
	reg = sdio_readb(func, IO_PORT_1, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s can't read 0x%x\n", __func__, IO_PORT_1);
		goto exit;
	}
	ioport |= ((reg & 0xff) << 8);

	sdio_claim_host(func);
	reg = sdio_readb(func, IO_PORT_2, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s can't read 0x%x\n", __func__, IO_PORT_2);
		goto exit;
	}
	ioport |= ((reg & 0xff) << 16);

	return ioport;
exit:
	return -1;
}

/*
	asr_sdio_enable_int: enable host interrupt
*/
static int asr_sdio_enable_int(struct sdio_func *func, int mask)
{
	u8 reg_val;
	int ret = -1;

	if (!(mask & HOST_INT_MASK)) {
		pr_info("%s 0x%x is invalid int mask\n", __func__, mask);
		return ret;
	}

	/* enable specific interrupt */
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

static void asr_sdio_disable_int(struct sdio_func *func, int mask)
{
	u8 reg_val;
	int ret = -1;

	/*enable specific interrupt */
	sdio_claim_host(func);
	reg_val = sdio_readb(func, HOST_INTMASK, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s read host int mask failed\n", __func__);
		return;
	}

	reg_val &= ~mask;
	sdio_claim_host(func);
	sdio_writeb(func, reg_val, HOST_INTMASK, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s disable host int mask failed\n", __func__);
	}
}

/*
    poll_card_status: check if card in ready state
*/
static int poll_card_status(struct sdio_func *func, u8 mask)
{
	u32 tries;
	u8 reg_val;
	int ret;

	for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
		sdio_claim_host(func);
		reg_val = sdio_readb(func, C2H_INTEVENT, &ret);
		sdio_release_host(func);
		if (ret) {
			pr_info("%s can't read 0x%x out\n", __func__, C2H_INTEVENT);
			break;
		}
		pr_info("%s reg_val: 0x%x mask: 0x%x\n", __func__, reg_val, mask);
		if ((reg_val & mask) == mask)
			return 0;
		msleep(10);
	}

	return -1;
}

/*
    check_scratch_status: check scratch register value
*/
static int check_scratch_status(struct sdio_func *func, u16 status)
{
	u32 tries;
	u8 *scratch_val = NULL;
	int ret;

	scratch_val = devm_kzalloc(&func->dev, 4, GFP_KERNEL | GFP_DMA);
	if (!scratch_val) {
		pr_info("%s can't alloc %d memory\n", __func__, 4);
		return -1;
	}

	for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
		sdio_claim_host(func);
		ret = sdio_readsb(func, scratch_val, SCRATCH_0, 4);
		sdio_release_host(func);
		if (ret) {
			pr_info("%s can't read 0x%x out\n", __func__, SCRATCH_0);
			break;
		}

		pr_info("%s scratch_val1: 0x%x status: 0x%x\n", __func__, *(u32*)scratch_val, status);
		if ((*(u32*)scratch_val & status) == status) {
			if (scratch_val) {
				devm_kfree(&func->dev, scratch_val);
				scratch_val = NULL;
			}
			return 0;
		}
		msleep(100);
	}

	if (scratch_val) {
		devm_kfree(&func->dev, scratch_val);
		scratch_val = NULL;
	}

	return -1;
}

#if defined(CONFIG_ASR5505) || defined(CONFIG_ASR595X) || defined(CONFIG_ASR5825)
static u32 asr_recursive_crc32(u32 init_crc, const u8 * buf, u32 len)
{
	u32 i, crc;

	crc = init_crc;
	while (len > 0 && len--) {
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
#endif

#ifdef CONFIG_ASR5531
int asr_sdio_send_section_firmware(struct sdio_func *func, struct sdio_host_sec_fw_header
											*p_sec_fw_hdr, u8 * fw_buf, u8 * fw)
{
	int retry_times = 3;
	int ret = -1;

	do {
		/* step 0 polling if card is in rdy */
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
			pr_info("%s card is not in rdy\n", __func__);
			break;
		}

		/* step 1 send out header data */
		memmove(fw_buf, p_sec_fw_hdr, sizeof(struct sdio_host_sec_fw_header));
		sdio_claim_host(func);
		ret = sdio_writesb(func, ioport, fw_buf, sizeof(struct sdio_host_sec_fw_header));
		sdio_release_host(func);
		if (ret) {
			pr_info("%s write header data failed %d\n", __func__, ret);
			break;
		}

		/* step 2 send out fw data */
		do {
			memmove(fw_buf, fw, p_sec_fw_hdr->sec_fw_len);
			ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
			if (ret) {
				pr_info("%s card is not in rdy\n", __func__);
				break;
			}

			sdio_claim_host(func);
			ret = sdio_writesb(func, ioport, fw_buf, p_sec_fw_hdr->sec_fw_len);
			sdio_release_host(func);
			if (ret) {
				pr_info("%s can't write %d data into card %d\n", __func__, p_sec_fw_hdr->sec_fw_len, ret);
				break;
			}
		} while (0);

		/* step 3 check fw_crc status */
		if (check_scratch_status(func, CRC_SUCCESS) == 0) {
			pr_info("%s CRC_SUCCESS: 0x%x\n", __func__, CRC_SUCCESS);
			/* reset the fw_crc status to check next section */
			sdio_claim_host(func);
			sdio_writeb(func, sdio_readb(func, SCRATCH_1, 0) & (~(CRC_SUCCESS >> 8)), SCRATCH_1, &ret);// clear bit14
			sdio_release_host(func);
			if (ret) {
				pr_info("%s reset fw_crc status fail!!! (%d)\n", __func__, ret);
			} else  {
				ret = 0;
				break;
			}
		}

		retry_times--;
		pr_info("%s retry %d times\n", __func__, 3 - retry_times);
	} while (retry_times > 0);

	return ret;
}

int asr_sdio_download_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	u8 *tempbuf = NULL;
	u8 *fw_buf = NULL;
	u8 *fw = NULL;
	int ret = -1;
	struct sdio_host_sec_fw_header sdio_host_sec_fw_header = {0};
	u32 jump_addr, total_sec_num, fw_tag, sec_idx, fw_hybrid_header_len, fw_sec_total_len, fw_dled_len, sec_total_len;
	u8 fw_sec_num;

	/* parse fw header and get sec num */
	memcpy(&fw_tag, fw_img->data, sizeof(u32));

	if (0xAABBCCDD != fw_tag) {
		pr_info("%s fw tag mismatch(0x%x 0x%x)\n", __func__, 0xAABBCCDD, fw_tag);
		return -1;
	}

	memcpy(&total_sec_num, fw_img->data + 4, sizeof(u32));
	fw_sec_num = (u8) total_sec_num;

	/* get the chip address of first section as jump address */
	memcpy(&sdio_host_sec_fw_header, fw_img->data + 8, sizeof(struct sdio_fw_sec_fw_header));
	jump_addr = sdio_host_sec_fw_header.chip_ram_addr;

	/* write sec num to scra register for fw read */
	sdio_claim_host(func);
	sdio_writeb(func, fw_sec_num, SCRATCH0_3, &ret);
	/* write the jump address to scratch 1 register */
	sdio_writeb(func, jump_addr & 0xFF, SCRATCH1_0, &ret);
	sdio_writeb(func, (jump_addr >> 8) & 0xFF, SCRATCH1_1, &ret);
	sdio_writeb(func, (jump_addr >> 16) & 0xFF, SCRATCH1_2, &ret);
	sdio_writeb(func, (jump_addr >> 24) & 0xFF, SCRATCH1_3, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s write fw_sec_num  fail!!! (%d)\n", __func__, ret);
		return -1;
	}
	// tag + sec num + [len/crc/addr]...
	fw_hybrid_header_len = 8 + total_sec_num * sizeof(struct sdio_fw_sec_fw_header);

	fw_sec_total_len = fw_img->size - fw_hybrid_header_len;
	if (fw_sec_total_len % 4) {
		pr_info("%s error: fw bin length not 4byte allignment\n", __func__);
		return -1;
	}

	pr_info("%s hybrid hdr (0x%x %d %d %d)!!\n", __func__, fw_tag,
		total_sec_num, fw_hybrid_header_len, fw_sec_total_len);

	sec_idx = 0;
	fw_dled_len = 0;
	do {
		// prepare sdio_host_sec_fw_header
		memcpy(&sdio_host_sec_fw_header,
		       fw_img->data + 8 +
		       sec_idx * sizeof(struct sdio_fw_sec_fw_header), sizeof(struct sdio_fw_sec_fw_header));

		if (sdio_host_sec_fw_header.sec_fw_len >= SDIO_BLOCK_SIZE_DLD * 511) {
			pr_info("%s sec bin size(%d) too big !!\n", __func__, sdio_host_sec_fw_header.sec_fw_len);
			goto exit;
		}

		if ((sdio_host_sec_fw_header.sec_fw_len > SDIO_BLOCK_SIZE_DLD)
		    && (sdio_host_sec_fw_header.sec_fw_len % SDIO_BLOCK_SIZE_DLD)) {
			sdio_host_sec_fw_header.transfer_unit =
			    sdio_host_sec_fw_header.sec_fw_len -
			    (sdio_host_sec_fw_header.sec_fw_len % SDIO_BLOCK_SIZE_DLD);
			sdio_host_sec_fw_header.transfer_times = 2;	// block + byte transmit
		} else {
			sdio_host_sec_fw_header.transfer_unit = sdio_host_sec_fw_header.sec_fw_len;
			sdio_host_sec_fw_header.transfer_times = 1;	// only block transmit
		}

		pr_info("%s idx(%d),dled_len=%d,sec headers: 0x%x-0x%x-0x%x-0x%x-0x%x\n", __func__,
			sec_idx, fw_dled_len,
			sdio_host_sec_fw_header.sec_fw_len,
			sdio_host_sec_fw_header.sec_crc,
			sdio_host_sec_fw_header.chip_ram_addr,
			sdio_host_sec_fw_header.transfer_unit, sdio_host_sec_fw_header.transfer_times);

		// sec bin total len.
		sec_total_len = sdio_host_sec_fw_header.sec_fw_len + sizeof(struct sdio_host_sec_fw_header);

		tempbuf = devm_kzalloc(&func->dev, sec_total_len + SDIO_BLOCK_SIZE_DLD, GFP_KERNEL | GFP_DMA);
		if (!tempbuf) {
			pr_info("%s can't alloc %d memory\n", __func__, sec_total_len + SDIO_BLOCK_SIZE_DLD);
			goto exit;
		}
		fw_buf = (u8 *) (ALIGN_ADDR(tempbuf, DMA_ALIGNMENT));	//DMA aligned buf for sdio transmit

		fw = (u8 *) devm_kzalloc(&func->dev, sdio_host_sec_fw_header.sec_fw_len, GFP_KERNEL);
		if (!fw) {
			pr_info("%s can't alloc %d memory\n", __func__, sdio_host_sec_fw_header.sec_fw_len);
			goto exit;
		}
		memcpy(fw, fw_img->data + fw_hybrid_header_len + fw_dled_len, sdio_host_sec_fw_header.sec_fw_len);

		if (asr_sdio_send_section_firmware(func, &sdio_host_sec_fw_header, fw_buf, fw)) {
			// any section fail,exit
			pr_info("%s sec(%d) download fail!\n", __func__, sec_idx);
			goto exit;
		}
		// download success, free buf
		if (tempbuf) {
			devm_kfree(&func->dev, tempbuf);
			tempbuf = NULL;
		}

		if (fw) {
			devm_kfree(&func->dev, fw);
			fw = NULL;
		}
		// caculate downloaded bin length.
		fw_dled_len += sdio_host_sec_fw_header.sec_fw_len;
		sec_idx++;
	} while (sec_idx < total_sec_num);

	if (fw_dled_len != fw_sec_total_len) {
		pr_info("%s fw len mismatch(%d %d)\n", __func__, fw_dled_len, fw_sec_total_len);
		return -1;
	}

	/* step 5 check fw runing status */
	if (check_scratch_status(func, BOOT_SUCCESS) < 0)
		goto exit;

	pr_info("%s BOOT_SUCCESS: 0x%x\n", __func__, BOOT_SUCCESS);

	return 0;

exit:
	if (tempbuf) {
		devm_kfree(&func->dev, tempbuf);
		tempbuf = NULL;
	}
	if (fw) {
		devm_kfree(&func->dev, fw);
		fw = NULL;
	}

	return -1;
}

#elif defined(CONFIG_ASR5505) || defined(CONFIG_ASR595X)
static int asr_sdio_send_fw(struct sdio_func *func, const struct firmware *fw_img,
								u8 *fw_buf, int blk_size, u32 total_len, u32 pad_len)
{
	int i = 0;
	int ret = 0;
	u8 padding_data[4] = {0};
	u32 fw_crc = ~0xffffffff; // crc init value
	u32 offset = 0;

	while (i < fw_img->size / blk_size ){
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
		    pr_info("%s get ioport failed\n", __func__);
		    return ret;
		}

		memcpy(fw_buf, fw_img->data + i * blk_size, blk_size);
		fw_crc = asr_recursive_crc32(~fw_crc, fw_buf, blk_size);
		sdio_claim_host(func);
		ret = sdio_writesb(func, ioport, fw_buf, blk_size);
		sdio_release_host(func);
		if (ret) {
		    pr_info("%s can't write %d data into card %d\n", __func__, total_len, ret);
		    return ret;
		}

		i++;
	}

	if (!(total_len - i * blk_size || pad_len))
		return ret;

	memset(fw_buf, 0, blk_size);
	offset = 0;
	pr_info("%s fw_img->size: %ld, len: %d\n", __func__, fw_img->size, total_len - i * blk_size);

	ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
	if (ret) {
		pr_info("%s card is not in rdy\n", __func__);
		return ret;
	}

	if (fw_img->size - i * blk_size != 0) {
		memcpy(fw_buf, fw_img->data + i * blk_size, fw_img->size - i * blk_size);
		fw_crc = asr_recursive_crc32(~fw_crc, fw_buf, fw_img->size - i * blk_size);
		offset = fw_img->size - i * blk_size;
	}

	if (pad_len) {
		memcpy(fw_buf + offset, padding_data, pad_len);
		fw_crc = asr_recursive_crc32(~fw_crc, fw_buf + offset, pad_len);
		offset += pad_len;
	}

	pr_info("%s crc: 0x%x\n", __func__, fw_crc);
	memcpy(fw_buf + offset, (u8 *)&fw_crc, sizeof(fw_crc));

	sdio_claim_host(func);
	ret = sdio_writesb(func, ioport, fw_buf, blk_size);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s can't write %d data into card %d\n", __func__, total_len, ret);
		return ret;
	}

	return ret;
}

static int asr_sdio_download_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	int blk_size;
	int ret = 0;
	u32 fw_len, total_len, pad_len, crc_len;
	u32 header_data[3];
	u8 *fw_buf = NULL;

	blk_size = SDIO_BLOCK_SIZE_DLD * 2; // / 2 to change to byte mode, * 2 change to block mode

	/* step 0 prepare header data and other initializations */
	fw_len = fw_img->size;
	pad_len = (fw_len % 4) ? (4 - (fw_len % 4)) : 0;
	crc_len = 4;
	total_len = fw_len + pad_len + crc_len;
	total_len = ((total_len + blk_size - 1) & (~(blk_size - 1)));
	header_data[0] = fw_len + pad_len; // actual length of fw
	header_data[1] = blk_size; // transfer unit
	header_data[2] = total_len / blk_size; // transfer times

	fw_buf = devm_kzalloc(&func->dev, blk_size + SDIO_BLOCK_SIZE_DLD, GFP_KERNEL | GFP_DMA);
	if (!fw_buf) {
		pr_info("%s can't alloc %d memory\n", __func__, blk_size + SDIO_BLOCK_SIZE_DLD);
		ret = -1;
		goto exit;
	}

	do {
		/* step 1 polling if card is in rdy */
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
			pr_info("%s card is not in rdy\n", __func__);
			goto exit;
		}

		pr_info("%s blk_size: %d total_len: %d pad_len: %d headers: 0x%x-0x%x-0x%x\n", __func__,
				blk_size, total_len, pad_len, header_data[0], header_data[1], header_data[2]);

		/* step 2 send out header data */
		sdio_claim_host(func);
		ret = sdio_writesb(func, ioport, header_data, sizeof(header_data));
		sdio_release_host(func);
		if (ret) {
			pr_info("%s write header data failed %d\n", __func__, ret);
			goto exit;
		}

		/* step 3 send out fw data */
		ret = asr_sdio_send_fw(func, fw_img, fw_buf, blk_size, total_len, pad_len);
		if (ret) {
			pr_info("%s can't write %d data into card %d\n", __func__, total_len, ret);
			goto exit;
		}

		/* step 4 check fw_crc status */
		if (check_scratch_status(func, CRC_SUCCESS) < 0){
			ret = -1;
			goto exit;
		}
		pr_info("%s CRC_SUCCESS: 0x%x\n", __func__, CRC_SUCCESS);

		/* step 5 check fw runing status */
		if (check_scratch_status(func, BOOT_SUCCESS) < 0){
			ret = -1;
			goto exit;
		}
		pr_info("%s BOOT_SUCCESS: 0x%x\n", __func__, BOOT_SUCCESS);
	} while (0);

	sdio_claim_host(func);
	sdio_writeb(func, 0x0, SCRATCH_0, &ret);
	sdio_writeb(func, 0x0, SCRATCH_1, &ret);
	sdio_release_host(func);

exit:
	if (fw_buf) {
		devm_kfree(&func->dev, fw_buf);
		fw_buf = NULL;
	}

	return ret;
}

#elif defined(CONFIG_ASR5825)

#define ASR_SND_BOOT_LOADER_SIZE   1024

/*
    Download fw README
    1, lega/duet chip integrate romcode v1.0, which support download directly into one continuous "ram" region
    2, canon chip intergrate romcode v2.0, which support download different sections of firmware into non-continuous "ram" region

    Now duet with wifi + ble function's firmware have different sections, and its "ram" region is non-continuous,
    but it integrated the romcode v1.0. so for duet support non-continuous "ram" region,
    it needs using 1st_firmware download the snd_bootloader, then using 2nd_firmware download work with snd_bootloader
    to download the total firmware with different section into different "ram" region.
*/
int asr_sdio_download_1st_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	int ret = 0;
	u32 fw_len, total_len, pad_len, crc_len;
	u32 fw_crc = ~0xffffffff; // crc init value
	u32 header_data[3];
	u8 *tempbuf = NULL;
	u8 *fw_buf = NULL;
	u8 padding_data[4] = {0};

	/* step 0 prepare header data and other initializations */
	fw_len = ASR_SND_BOOT_LOADER_SIZE;
	pad_len = (fw_len % 4) ? (4 - (fw_len % 4)) : 0;
	crc_len = 4;
	total_len = fw_len + pad_len + crc_len;
	total_len = ASR_ALIGN_DLDBLKSZ_HI(total_len);
	header_data[0] = ASR_SND_BOOT_LOADER_SIZE + pad_len;
	header_data[1] = total_len;
	header_data[2] = 1;

	tempbuf = devm_kzalloc(&func->dev, total_len + SDIO_BLOCK_SIZE_DLD, GFP_KERNEL | GFP_DMA);
	if (!tempbuf) {
		pr_info("%s can't alloc %d memory\n", __func__, total_len + SDIO_BLOCK_SIZE_DLD);
		ret = -1;
		goto exit;
	}
	fw_buf = (u8 *)(ALIGN_ADDR(tempbuf, DMA_ALIGNMENT));

	memcpy(fw_buf, fw_img->data, ASR_SND_BOOT_LOADER_SIZE);
	memcpy(fw_buf + ASR_SND_BOOT_LOADER_SIZE, padding_data, pad_len);
	fw_crc = asr_recursive_crc32(~fw_crc, fw_buf, ASR_SND_BOOT_LOADER_SIZE + pad_len);
	memcpy(fw_buf + ASR_SND_BOOT_LOADER_SIZE + pad_len, (u8 *)&fw_crc, crc_len);

	pr_info("%s fw_len: %d pad_len: %d crc_len: %d total_len: %d headers: 0x%x-0x%x-0x%x crc: 0x%x\n", __func__,
			fw_len, pad_len, crc_len, total_len, header_data[0], header_data[1], header_data[2], fw_crc);

	/* step 1 polling if card is in rdy */
	ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
	if (ret) {
		pr_info("%s card is not in rdy\n", __func__);
		goto exit;
	}

	/* step 2 send out header data */
	sdio_claim_host(func);
	ret = sdio_writesb(func, ioport, header_data, sizeof(header_data));
	sdio_release_host(func);
	if (ret) {
		pr_info("%s write header data failed %d\n", __func__, ret);
		goto exit;
	}

	/* step 3 send out fw data */
	ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
	if (ret) {
		pr_info("%s card is not in rdy\n", __func__);
		goto exit;
	}

	sdio_claim_host(func);
	ret = sdio_writesb(func, ioport, fw_buf, total_len);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s can't write %d data into card %d\n", __func__, total_len, ret);
		goto exit;
	}

	/* step 4 check fw_crc status */
	//if (check_scratch_status(func, CRC_SUCCESS) < 0){
	//	ret = -1;
	//	goto exit;
	//}
	//pr_info("%s CRC_SUCCESS: 0x%x\n", __func__, CRC_SUCCESS);

	/* step 5 check fw runing status */
	if (check_scratch_status(func, BOOT_SUCCESS) < 0){
		ret = -1;
		goto exit;
	}
	pr_info("%s BOOT_SUCCESS: 0x%x\n", __func__, BOOT_SUCCESS);

exit:
	if (tempbuf) {
		devm_kfree(&func->dev, tempbuf);
		tempbuf = NULL;
	}

	return ret;
}

static int asr_sdio_send_fw(struct sdio_func *func, uint8_t *fw_img_data, uint32_t fw_img_size,
								u8 *fw_buf, int blk_size, u32 total_len, u32 pad_len)
{
	int i = 0;
	int ret = 0;
	u8 padding_data[4] = {0};

	while (i < total_len / blk_size ){
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
			pr_info("%s card is not in rdy\n", __func__);
			return ret;
		}

		memcpy(fw_buf, fw_img_data + i * blk_size, blk_size);
		sdio_claim_host(func);
		ret = sdio_writesb(func, ioport, fw_buf, blk_size);
		sdio_release_host(func);
		if (ret) {
			pr_info("%s can't write %d data into card %d\n", __func__, total_len, ret);
			return ret;
		}

		i++;
	}

	if (!(total_len - i * blk_size || pad_len))
		return ret;

	memset(fw_buf, 0, blk_size);
	pr_info("%s fw_img_size: %d, len: %d", __func__, fw_img_size, total_len - i * blk_size);

	ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
	if (ret) {
		pr_info("%s card is not in rdy\n", __func__);
		return ret;
	}

	if (fw_img_size - i * blk_size != 0)
		memcpy(fw_buf, fw_img_data + i * blk_size, fw_img_size - i * blk_size);

	if (pad_len)
		memcpy(fw_buf + fw_img_size - i * blk_size, padding_data, pad_len);

	sdio_claim_host(func);
	ret = sdio_writesb(func, ioport, fw_buf, blk_size);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s can't write %d data into card %d\n", __func__, total_len, ret);
		return ret;
	}

	return ret;
}

int asr_sdio_send_section_firmware(struct sdio_func *func, struct sdio_host_sec_fw_header
											*p_sec_fw_hdr, u8 * fw_buf, u8 * fw, int blk_size)
{
	int retry_times = 3;
	int ret = -1;

	do {
		/* step 0 polling if card is in rdy */
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
			pr_info("%s card is not in rdy\n", __func__);
			break;
		}

		/* step 1 send out header data */
		memmove(fw_buf, p_sec_fw_hdr, sizeof(struct sdio_host_sec_fw_header));
		sdio_claim_host(func);
		ret = sdio_writesb(func, ioport, fw_buf, sizeof(struct sdio_host_sec_fw_header));
		sdio_release_host(func);
		if (ret) {
			pr_info("%s write header data failed %d\n", __func__, ret);
			break;
		}

		/* step 2 send out fw data */
		ret = asr_sdio_send_fw(func, fw, p_sec_fw_hdr->sec_fw_len, fw_buf, blk_size, p_sec_fw_hdr->sec_fw_len, 0);
		if (ret) {
			pr_info("%s can't write %d data into card %d\n", __func__, p_sec_fw_hdr->sec_fw_len, ret);
			break;
		}

		/* step 3 check fw_crc status */
		if (check_scratch_status(func, CRC_SUCCESS) == 0) {
			pr_info("%s CRC_SUCCESS: 0x%x\n", __func__, CRC_SUCCESS);
			ret = 0;
			break;
		}

		retry_times--;
		pr_info("%s retry %d times\n", __func__, 3 - retry_times);
	} while (retry_times > 0);

	return ret;
}

int asr_sdio_download_2nd_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	int blk_size;
	int ret = 0;
	u8 *fw_buf = NULL;
	u8 *tempbuf = NULL;
	const u8 *fw_data;
	size_t fw_size;
	struct sdio_host_sec_fw_header sdio_host_sec_fw_header = {0};
	u32 total_sec_num, fw_tag, sec_idx, fw_hybrid_header_len, fw_sec_total_len, fw_dled_len, sec_total_len;
	u8 fw_sec_num;

	blk_size = SDIO_BLOCK_SIZE_DLD * 2; // / 2 to change to byte mode, * 2 change to block mode

	/* parse fw header and get sec num */
	fw_data = fw_img->data + ASR_SND_BOOT_LOADER_SIZE;
	fw_size = fw_img->size - ASR_SND_BOOT_LOADER_SIZE;
	memcpy(&fw_tag, fw_data, sizeof(u32));

	if (0xAABBCCDD != fw_tag) {
		pr_info("%s fw tag mismatch(0x%x 0x%x)\n", __func__, 0xAABBCCDD, fw_tag);
		ret = -1;
		goto exit;
	}
	memcpy(&total_sec_num, fw_data + 4, sizeof(u32));
	fw_sec_num = (u8) total_sec_num;
	// write sec num to scra register for fw read
	sdio_claim_host(func);
	sdio_writeb(func, fw_sec_num, SCRATCH0_3, &ret);
	sdio_release_host(func);
	if (ret) {
		pr_info("%s write fw_sec_num fail!!! (%d)\n", __func__, ret);
		ret = -1;
		goto exit;
	}
	// tag + sec num + [len/crc/addr]...+
	fw_hybrid_header_len = 8 + total_sec_num * sizeof(struct sdio_fw_sec_fw_header);
	fw_sec_total_len = fw_size - fw_hybrid_header_len;
	if (fw_sec_total_len % 4) {
		pr_info("%s error: fw bin length not 4byte allignment\n", __func__);
		ret = -1;
		goto exit;
	}
	pr_info("%s hybrid hdr (0x%x %d %d %d)!!\n", __func__, fw_tag,
			total_sec_num, fw_hybrid_header_len, fw_sec_total_len);

	sec_idx = 0;
	fw_dled_len = 0;
	do {
		// prepare sdio_host_sec_fw_header
		memcpy(&sdio_host_sec_fw_header,
		       fw_data + 8 +
		       sec_idx * sizeof(struct sdio_fw_sec_fw_header), sizeof(struct sdio_fw_sec_fw_header));

		sdio_host_sec_fw_header.transfer_unit = blk_size;
		sdio_host_sec_fw_header.transfer_times = sdio_host_sec_fw_header.sec_fw_len / blk_size;

		if (sdio_host_sec_fw_header.sec_fw_len % blk_size) {
			sdio_host_sec_fw_header.transfer_times++;
		}
		pr_info("%s idx(%d),dled_len=%d,sec headers: 0x%x-0x%x-0x%x-0x%x-0x%x\n", __func__,
				sec_idx, fw_dled_len,
				sdio_host_sec_fw_header.sec_fw_len,
				sdio_host_sec_fw_header.sec_crc,
				sdio_host_sec_fw_header.chip_ram_addr,
				sdio_host_sec_fw_header.transfer_unit, sdio_host_sec_fw_header.transfer_times);

		// sec bin total len.
		sec_total_len = sdio_host_sec_fw_header.sec_fw_len + sizeof(struct sdio_host_sec_fw_header);

		tempbuf = devm_kzalloc(&func->dev, blk_size * 2, GFP_KERNEL | GFP_DMA);
		if (!tempbuf) {
			pr_info("%s can't alloc %d memory\n", __func__, sec_total_len + SDIO_BLOCK_SIZE_DLD);
			ret = -1;
			goto exit;
		}
		fw_buf = (u8 *) (ALIGN_ADDR(tempbuf, DMA_ALIGNMENT));
		if (asr_sdio_send_section_firmware(func, &sdio_host_sec_fw_header, fw_buf, (u8 *)(fw_data + fw_hybrid_header_len + fw_dled_len), blk_size)) {
			// any section fail,exit
			pr_info("%s sec(%d) download fail!\n", __func__, sec_idx);
			ret = -1;
			goto exit;
		}
		// download success, free buf
		if (tempbuf) {
			devm_kfree(&func->dev, tempbuf);
			tempbuf = NULL;
		}
		// caculate downloaded bin length.
		fw_dled_len += sdio_host_sec_fw_header.sec_fw_len;
		sec_idx++;
	} while (sec_idx < total_sec_num);

	if (fw_dled_len != fw_sec_total_len) {
		pr_info("%s fw len mismatch(%d %d)\n", __func__, fw_dled_len, fw_sec_total_len);
		ret = -1;
		goto exit;
	}

	if (check_scratch_status(func, BOOT_SUCCESS) < 0){
		ret = -1;
		goto exit;
	}

	msleep(20);
	pr_info("%s BOOT_SUCCESS: 0x%x\n", __func__, BOOT_SUCCESS);

	ret = poll_card_status(func, C2H_IO_RDY);
	if (ret) {
		pr_info("%s card is not in rdy\n", __func__);
		ret = -1;
		goto exit;
	}

exit:
	if (tempbuf) {
		devm_kfree(&func->dev, tempbuf);
		tempbuf = NULL;
	}

	return ret;
}
#endif

/*
	download_firmware: download firmware into card
	total data format will be: | header data | fw_data | padding | CRC |(CRC check fw_data + padding)
	header data will be: | fw_len | transfer unit | transfer times
*/
static int download_firmware(struct sdio_func *func, char *filename)
{
	const struct firmware *fw_img = NULL;
	int ret = 0;

	ret = request_firmware(&fw_img, filename, &func->dev);
	if (ret) {
		pr_info("%s request firmware failed ret: %d\n", __func__, ret);
		return ret;
	}
	pr_info("request firmware: %s, fw_len: %ld\n", filename, fw_img->size);

#ifdef CONFIG_ASR5825
	/* download 2nd bootloader work with romcode v1.0 */
	if (asr_sdio_download_1st_firmware(func, fw_img) < 0)
		ret = -1;
	/* download the hybrid firmware work with the 2nd bootloader */
	if (asr_sdio_download_2nd_firmware(func, fw_img) < 0)
		ret = -1;
#else
	/* downlodad firmware using sdio */
	if (asr_sdio_download_firmware(func, fw_img) < 0)
		ret = -1;
#endif

	release_firmware(fw_img);
	return ret;
}

static int asr_sdio_readw(struct sdio_func *func, uint32_t addr, uint16_t *data)
{
	int ret = 0;
	uint8_t  val_b;

	sdio_claim_host(func);
	val_b = sdio_readb(func, addr, &ret);
	sdio_release_host(func);
	if (ret) {
		loopback_log("Read addr 0x%x low failed", addr);
		return -ENODEV;
	}
	*data = (val_b & 0xFF);

	sdio_claim_host(func);
	val_b = sdio_readb(func, addr + 1, &ret);
	sdio_release_host(func);
	if (ret) {
		loopback_log("Read addr 0x%x failed", addr + 1);
		return -ENODEV;
	}
	*data |= ((val_b & 0xFF) << 8);

	//loopback_log("read addr 0x%x val: 0x%x", addr, *data);

	return 0;
}

static void trigger_slaver_do_rxtest(struct sdio_func *func)
{
	int ret = 0;
	uint8_t reg_val = 0;

	reg_val = (0x1 << 3);
	sdio_claim_host(func);
	sdio_writeb(func, reg_val, H2C_INTEVENT, &ret);
	sdio_release_host(func);

	pr_info("Send H2C_INTEVENT Host_to_Card_Event done, ret: %d\n", ret);

	return;
}

static int read_write_single_port_test(struct sdio_func *func, bool write)
{
	int ioport, ret;
	uint32_t bitmap_addr, addr;
	u16 port_index, bitmap;

	/* step 0: get ioport */
	ioport = sdio_get_ioport(func);
	if (ioport < 0) {
		loopback_log("get ioport failed");
		return -ENODEV;
	}
	ioport &= (0x1e000); // port address occpy [17:14]

	/* step 1: wait for card rdy now only using irq way */
	/* Check card status by polling or interrupt mode */
	if (start_flag) {
		if (is_irq == 1) {
			if (write) {
				if (0 == wait_for_completion_timeout(&dnld_completion, msecs_to_jiffies(1000))) {
					loopback_log("wait to write timeout 1s");
					return -ETIMEDOUT;
				}
			} else {
				if (0 == wait_for_completion_timeout(&upld_completion, msecs_to_jiffies(1000))) {
					loopback_log("wait to read timeout 1s");
					return -ETIMEDOUT;
				}
			}
		} else if (is_irq == 2) {
			loopback_log("can't support now");
		} else {
			loopback_log("polling card status");
			ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
			if (ret) {
				loopback_log("card is not rdy!");
				return -ENODEV;
			}
		}
	} else {
		start_flag = 1;
		loopback_log("first polling card status");
		ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
		if (ret) {
			loopback_log("card is not rdy!");
			start_flag = 0;
			return -ENODEV;
		}
	}

	/* step 2: read bitmap to get which port is for opeartion as single port test only one port is valid */
	if (write)
		bitmap_addr = WR_BITMAP_L;
	else
		bitmap_addr = RD_BITMAP_L;

	ret = asr_sdio_readw(func, bitmap_addr, &bitmap);
	if (ret) {
		loopback_log("read bit map failed");
		return -ENODEV;
	}

	port_index = ffs(bitmap) - 1;
	loopback_log("bitmap: %#x, port: %d", bitmap, port_index);

	// read buffer size
	if (!write) {
		ret = asr_sdio_readw(func, RD_LEN_L(port_index), &single_buf_size);
		if (ret) {
			loopback_log("read buffer size failed");
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
			loopback_log("write failed, buf is null or buf len is 0");
			return -ENODEV;
		}
	}

	/* step 3: do a read or write operation */
	addr = ioport | (port_index);
	loopback_log("CMD53 %s port:%d (bitmap:0x%x) addr:0x%x buf len:%d (from rd len register[%d]:0x%x)",
				write ? "W ->" : "R <-", port_index, bitmap, addr, single_buf_size, port_index, RD_LEN_L(port_index));

	sdio_claim_host(func);
	ret = (write ? sdio_writesb(func, addr, single_buf, (int)single_buf_size) : sdio_readsb(func, single_buf, addr, (int)single_buf_size));
	sdio_release_host(func);
	if (ret) {
		loopback_log("%s data failed", write ? "write" : "read");
		return ret;
	}

	loopback_log("%s Done", write ? "W" : "R");

#if 0
	if (!write)
		print_hex_dump(KERN_DEBUG, "RX: ", DUMP_PREFIX_OFFSET, 16, 1, single_buf, single_buf_size, true);
#endif

	/* free buffer after write cycle */
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
	int block_size_arr[] = {512, 128, 32}; // same in device side
	int loop, test_times, ret;

	reinit_completion(&upld_completion);
	reinit_completion(&dnld_completion);

	loopback_log("start single port test...");

	/* loop 3 different block size with each port */
	for (loop = 0; loop < ARRAY_SIZE(block_size_arr); loop++) {
		sdio_claim_host(func);
		ret = sdio_set_block_size(func, block_size_arr[loop]);
		sdio_release_host(func);
		if (ret) {
			loopback_log("set block size %d failed", block_size_arr[loop]);
			goto exit;
		}

		for (test_times = 0; test_times < 16; test_times++) {
			loopback_log("single port test, block size: %d, cycle: %d", block_size_arr[loop], test_times);

			ret = read_write_single_port_test(func, 0);
			if (ret) {
				loopback_log("read single port test failed");
				goto exit;
			}
			ret = read_write_single_port_test(func, 1);
			if (ret) {
				loopback_log("write single port test failed");
				goto exit;
			}
		}
	}

	pr_info("*******************************************\r\n"
			"*********** single port test done! ********\r\n"
			"*******************************************\r\n");

	return 0;

exit:
	if (single_buf) {
		kfree(single_buf);
		single_buf = NULL;
		single_buf_size = 0;
	}
	return ret;
}

static int read_write_multi_port_test(struct sdio_func *func, bool write)
{
	int ioport, ret, i, j, port_used;
	uint32_t bitmap_addr, addr;
	u16 port_size, bitmap, port_index;

	/* step 0: get ioport */
	ioport = sdio_get_ioport(func);
	if (ioport < 0) {
		loopback_log("get ioport failed");
		return -ENODEV;
	}
	ioport &= (0x1e000);

	/* step 1: wait for card rdy */
	if (start_flag) {
		if (is_irq == 1) {
			if (write) {
				if (0 == wait_for_completion_timeout(&dnld_completion, msecs_to_jiffies(1000))) {
					loopback_log("wait to write timeout 1s");
					return -ETIMEDOUT;
				}
			} else {
				if (0 == wait_for_completion_timeout(&upld_completion, msecs_to_jiffies(1000))) {
					loopback_log("wait to read timeout 1s");
					return -ETIMEDOUT;
				}
			}
		} else if (is_irq == 2) {
			loopback_log("can't support now");
		} else {
			loopback_log("polling card status");
			ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
			if (ret) {
				loopback_log("card is not rdy!");
				return -ENODEV;
			}
		}
	} else {
		start_flag = 1;
		loopback_log("first polling card status");
		ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
		if (ret) {
			loopback_log("card is not rdy!");
			start_flag = 0;
			return -ENODEV;
		}
	}

	/* step 2: read bitmap to know which ports are for opeartion */
	if (write)
		bitmap_addr = WR_BITMAP_L;
	else
		bitmap_addr = RD_BITMAP_L;

	ret = asr_sdio_readw(func, bitmap_addr, &bitmap);
	if (ret) {
		loopback_log("read bit map failed");
		return -ENODEV;
	}

	port_index = ffs(bitmap) - 1;
	loopback_log("bitmap: %#x, port: %d", bitmap, port_index);

	port_used = 0;

	/* step 3: do multi read or write operation, need make sure read before write */
	for (i = 0; i < 16; i++) {
		// in test phase rd_bitmap can not match the wr_bitmap, but from right to left to assign the buffer to write
		// find each free port
		if (bitmap & (1 << i)) {
			if (write) {// write
				addr = ioport | i;
				port_size  = multi_port_size[port_used];

				sdio_claim_host(func);
				ret = sdio_writesb(func, addr, multi_port_buffer[port_used], port_size);
				sdio_release_host(func);
				if (ret) {
					loopback_log("write data failed, cycle: %d", i);
					return ret;
				}

				// free buffer
				kfree(multi_port_buffer[port_used]);
				multi_port_buffer[port_used] = NULL;
				multi_port_size[port_used] = 0;
				port_used++;
			} else {// read
				ret = asr_sdio_readw(func, RD_LEN_L(i), &port_size);
				if (ret) {
					loopback_log("multi port read RD_LEN_L failed");
					return -ENODEV;
				}

				if (port_size == 0) {
					loopback_log("read port(%d) len is 0, error!", i);
					return -ENODEV;
				}

				// alloc buffer
				multi_port_buffer[port_used] = kzalloc((size_t)port_size, GFP_KERNEL);
				multi_port_size[port_used] = port_size;
				if (!multi_port_buffer[port_used]) {
					for (j = 0; j < port_used; j++) {
						if (multi_port_buffer[j])
							kfree(multi_port_buffer[j]);
					}
					loopback_log("alloc memory failed during multi-port test!, cycle: %d", i);
					return -ENOMEM;
				}

				addr = ioport | i;

				sdio_claim_host(func);
				ret = sdio_readsb(func, multi_port_buffer[port_used], addr, port_size);
				sdio_release_host(func);
				if (ret) {
					loopback_log("read data failed, cycle: %d", i);
					return ret;
				}
				port_used++;
			}

			loopback_log("CMD53 %s port:%d (bitmap:0x%x) addr:0x%x buf len:%d (from rd len register[%d]:0x%x) port_used:%d",
						write ? "W ->" : "R <-", i, bitmap, addr, port_size, i, RD_LEN_L(i), port_used);
		}
	}

	loopback_log("%s Done", write ? "W" : "R");

	return ret;
}

static int multi_port_tests(struct sdio_func *func)
{
	int block_size_arr[] = {512, 128, 32};
	int loop, test_times, ret, i;

	loopback_log("start multi port test...");

	for (loop = 0; loop < 16; loop++)
		multi_port_buffer[loop] = NULL;

	reinit_completion(&upld_completion);
	reinit_completion(&dnld_completion);

	for (loop = 0; loop < ARRAY_SIZE(block_size_arr); loop++) {
		sdio_claim_host(func);
		ret = sdio_set_block_size(func, block_size_arr[loop]);
		sdio_release_host(func);
		if (ret) {
			loopback_log("set block size %d failed", block_size_arr[loop]);
			goto exit;
		}

		for (test_times = 0; test_times < 16; test_times++) {
			loopback_log("multi port test, block size: %d, cycle: %d", block_size_arr[loop], test_times);

			ret = read_write_multi_port_test(func, 0);
			if (ret) {
				loopback_log("read multi port test failed");
				goto exit;
			}
			ret = read_write_multi_port_test(func, 1);
			if (ret) {
				loopback_log("write multi port test failed");
				goto exit;
			}
		}
	}

	pr_info("*******************************************\r\n"
			"********** multi port test done! **********\r\n"
			"*******************************************\r\n");

	return 0;

exit:
	for (i = 0; i < 16; i++) {
		if (multi_port_buffer[i]) {
			kfree(multi_port_buffer[i]);
			multi_port_buffer[i] = NULL;
			multi_port_size[i] = 0;
		}
	}
	return ret;
}

static int read_write_agg_port_test(struct sdio_func *func, int block_size, bool write)
{
	int ioport, ret;
	int agg_bitmap, i, agg_num;
	int start, end, port_used;
	uint32_t bitmap_addr, addr;
	u16 port_size, bitmap, start_port;

	/* step 0: get ioport */
	ioport = sdio_get_ioport(func);
	if (ioport < 0) {
		loopback_log("get ioport failed");
		return -EINVAL;
	}
	ioport &= (0x1e000);

	/* step 1: wait for card rdy */
	if (start_flag) {
		if (is_irq == 1) {
			if (write) {
				if (0 == wait_for_completion_timeout(&dnld_completion, msecs_to_jiffies(1000))) {
					loopback_log("wait to write timeout 1s");
					return -ETIMEDOUT;
				}
			} else {
				if (0 == wait_for_completion_timeout(&upld_completion, msecs_to_jiffies(1000))) {
					loopback_log("wait to read timeout 1s");
					return -ETIMEDOUT;
				}
			}
		} else if (is_irq == 2) {
			loopback_log("can't support now");
		} else {
			loopback_log("polling card status");
			ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
			if (ret) {
				loopback_log("card is not rdy!");
				return -ENODEV;
			}
		}
	} else {
		start_flag = 1;
		loopback_log("first polling card status");
		ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
		if (ret) {
			loopback_log("card is not rdy!");
			start_flag = 0;
			return -ENODEV;
		}
	}

	/* step 2: read bitmap to know which ports are for opeartion */
	if (write)
		bitmap_addr = WR_BITMAP_L;
	else
		bitmap_addr = RD_BITMAP_L;

	ret = asr_sdio_readw(func, bitmap_addr, &bitmap);
	if (ret) {
		loopback_log("read bit map failed");
		return -ENODEV;
	}

	if (write) {// write
		// [2byte length][data][padding] should in block size
		aggr_wr_size = 0;
		port_used = 0;
		loopback_log("aggr_wr_size = 0");
		for (i = 0; i < 16; i++) {
			if (bitmap & (1 << i)) {
				aggr_wr_size += aggr_port_size[port_used];
				loopback_log(" + %d", aggr_port_size[port_used]);
				// aligned with block size
				if ((aggr_port_size[port_used]) % block_size) {
					aggr_wr_size += (block_size - ((aggr_port_size[port_used]) % block_size));
					loopback_log(" + %d", (block_size - ((aggr_port_size[port_used]) % block_size)));
				}
				port_used++;
			}
		}
		loopback_log(";");
	} else {// read
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
					loopback_log("aggr port read RD_LEN failed");
					return -ENODEV;
				}

				loopback_log(" + %d (from [%d]:0x%x 0x%x)", port_size, i, RD_LEN_L(i), RD_LEN_U(i));

				// record each port size
				aggr_port_size[port_used] = port_size;
				aggr_port_size_print[i] = port_size;
				port_used++;
				aggr_rd_size += port_size;
			}
		}

		// add 4 fake size to store mark char
		aggr_rd_size += 4;
		loopback_log(";");
		aggr_rd_buf = kzalloc((size_t)aggr_rd_size, GFP_KERNEL);
		if (!aggr_rd_buf) {
			loopback_log("can't allock buffer for aggregation test, length: %d", aggr_rd_size);
			return -ENOMEM;
		}
	}

	// found the bitmap area of aggr mode
	start = ffs(bitmap);
	end = fls(bitmap);
	// assume that there is full '1' between ffs and fls, take no care other situations now
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
					  end        start

		2nd situation
	>fls - 1<                      >ffs - 1<
		 |                              |
		 --------------------------------
		 |/////|                    |///|
		 |/////|                    |///|
		 |/////|                    |///|
		 --------------------------------
			   ^                    ^
			   |                    |
			 start                end
	 */

	// 2nd situation
	if (end - start == 15) {
		int zero_start, zero_end;
		zero_start = ffs(~bitmap & 0xFFFF);
		zero_end = fls(~bitmap & 0xFFFF);
		start_port = zero_end;
		agg_num = (16 - (zero_end - zero_start + 1));
		agg_bitmap = ((1 << agg_num) - 1) & 0xFF;
		addr = ioport | (1 << 12) | (agg_bitmap << 4) | start_port;
	} else {// 1st situation
		start_port = start - 1;
		agg_num = (end - start + 1);
		agg_bitmap = ((1 << agg_num) - 1) & 0xFF;
		addr = ioport | (1 << 12) | (agg_bitmap << 4) | start_port;
	}

	if (write && aggr_wr_size == 192) {
		loopback_log("%s do a joke in write change size from %d to", __func__, aggr_wr_size);
		aggr_wr_size = 72;
		loopback_log("%d", aggr_wr_size);
	}

	/* step 3: do read or write operation */
	loopback_log("CMD53 %s start port:%d (bitmap:0x%x) port count:%d agg_bitmap:0x%x addr:0x%x buf_size:%d",
					write ? "W ->" : "R <-", start_port, bitmap, agg_num, agg_bitmap, addr, write ? aggr_wr_size : aggr_rd_size - 4);

	sdio_claim_host(func);
	ret = (write ? sdio_writesb(func, addr, aggr_rd_buf, aggr_wr_size) : sdio_readsb(func, aggr_rd_buf, addr, aggr_rd_size - 4));
	sdio_release_host(func);
	if (ret) {
		loopback_log("%s data failed", write ? "write" : "read");
		return ret;
	}

	loopback_log("%s Done", write ? "W" : "R");

	if (write) {
		u32 *ptemp;
		if (is_loopback_display) {
			int i, temp_port;
			ptemp = (u32*)aggr_rd_buf;
			for (i = 0; i < agg_num; i++) {
				temp_port = (start_port + i) % 16;
				loopback_log("%s write port[%d] with size:%d: first:0x%x - last:0x%x",
						__func__, temp_port, aggr_port_size_print[temp_port], *ptemp, *(ptemp + aggr_port_size_print[temp_port] / 4 - 1));
				ptemp += (aggr_port_size_print[temp_port] / 4);
			}
		}
		kfree(aggr_rd_buf);
		aggr_rd_buf = NULL;
		aggr_wr_size = 0;
	} else {
		u32 *ptemp;
		// add fake 0x12345678 at the rx_buf's tail
		ptemp = ((u32*)aggr_rd_buf) + (aggr_rd_size / 4 - 1);
		*ptemp = 0x12345678;

		if (is_loopback_display) {
			int i, temp_port;
			ptemp = (u32*)aggr_rd_buf;
			for (i = 0; i < agg_num; i++) {
				temp_port = (start_port + i) % 16;
				loopback_log("%s read port[%d] with size:%d: first:0x%x - last:0x%x",
						__func__, temp_port, aggr_port_size_print[temp_port], *ptemp, *(ptemp + aggr_port_size_print[temp_port] / 4 - 1));
				ptemp += (aggr_port_size_print[temp_port] / 4);
			}
		}
	}

	return ret;
}

static int asr_sdio_rw_extended_sg(struct sdio_func *func, int write,
	unsigned addr, int incr_addr, u8 **buf, u16 *buf_len, int buf_cnt, unsigned size)
{
	unsigned max_blocks, blksz, blocks;
	struct mmc_request mrq = {};
	struct mmc_command cmd = {};
	struct mmc_data data = {};
	struct scatterlist sg, *sg_ptr;
	struct sg_table sgtable;
	struct mmc_card *card = func->card;
	unsigned int i;
	unsigned int seg_size = card->host->max_seg_size;
	int err = 0;

	if (!func || (func->num > 7))
		return -EINVAL;

	if (addr & ~0x1FFFF)
		return -EINVAL;

	if (func->card->cccr.multi_block && (size >= func->cur_blksize)) {
		max_blocks = min(func->card->host->max_blk_count, 511u);
		blksz = func->cur_blksize;
		blocks = size / blksz;
		if (blocks > max_blocks) {
			asr_err("exceed max number of blocks\n");
			return -EINVAL;
		}
	} else {
		blksz = size;
		blocks = 0;
	}

	mrq.cmd = &cmd;
	mrq.data = &data;

	cmd.opcode = SD_IO_RW_EXTENDED;
	cmd.arg = write ? 0x80000000 : 0x00000000;
	cmd.arg |= func->num << 28;
	cmd.arg |= incr_addr ? 0x04000000 : 0x00000000;
	cmd.arg |= addr << 9;
	if (blocks == 0)
		cmd.arg |= (blksz == 512) ? 0 : blksz;	/* byte mode */
	else
		cmd.arg |= 0x08000000 | blocks;		/* block mode */
	cmd.flags = MMC_RSP_SPI_R5 | MMC_RSP_R5 | MMC_CMD_ADTC;

	data.blksz = blksz;
	/* Code in host drivers/fwk assumes that "blocks" always is >=1 */
	data.blocks = blocks ? blocks : 1;
	data.flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;

	data.sg_len = buf_cnt;
	if (data.sg_len > 1) {
		if (sg_alloc_table(&sgtable, data.sg_len, GFP_KERNEL))
			return -ENOMEM;

		data.sg = sgtable.sgl;
		sg_ptr = data.sg;
		for (i = 0; i < data.sg_len; i++)
		{
			if (buf_len[i] > seg_size) {
				asr_err("exceed max seg size\n");
				goto exit;
			}

			pr_info("buf 0x%x, len %d\n", (u32) (uintptr_t) buf[i], buf_len[i]);
			sg_set_buf(sg_ptr, buf[i], buf_len[i]);
			sg_ptr = sg_next(sg_ptr);
		}
	} else {
		if (buf_len[0] > seg_size) {
			asr_err("exceed max seg size\n");
			return -EINVAL;
		}
		data.sg = &sg;
		sg_init_one(&sg, buf[0], buf_len[0]);
	}

	mmc_set_data_timeout(&data, card);

	if (card->host->ops->pre_req)
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
		card->host->ops->pre_req(card->host, &mrq);
	#else
	    card->host->ops->pre_req(card->host, &mrq, true);
	#endif

	mmc_wait_for_req(card->host, &mrq);

	if (cmd.error)
		err = cmd.error;
	else if (data.error)
		err = data.error;
	else if (mmc_host_is_spi(card->host))
		/* host driver already reported errors */
		err = 0;
	else if (cmd.resp[0] & R5_ERROR)
		err = -EIO;
	else if (cmd.resp[0] & R5_FUNCTION_NUMBER)
		err = -EINVAL;
	else if (cmd.resp[0] & R5_OUT_OF_RANGE)
		err = -ERANGE;
	else
		err = 0;

	if (card->host->ops->post_req)
		card->host->ops->post_req(card->host, &mrq, err);

exit:
	if (data.sg_len > 1)
		sg_free_table(&sgtable);

	return err;
}

static int read_write_agg_port_test_sg(struct sdio_func *func, int block_size, bool write)
{
	int ioport, ret;
	int agg_bitmap, i, j, agg_num;
	int start, end, port_used;
	uint32_t bitmap_addr, addr;
	u16 port_size, bitmap, start_port;

	/* step 0: get ioport */
	ioport = sdio_get_ioport(func);
	if (ioport < 0) {
		loopback_log("get ioport failed");
		return -EINVAL;
	}
	ioport &= (0x1e000);

	/* step 1: wait for card rdy */
	if (start_flag) {
		if (is_irq == 1) {
			if (write) {
				if (0 == wait_for_completion_timeout(&dnld_completion, msecs_to_jiffies(1000))) {
					loopback_log("wait to write timeout 1s");
					return -ETIMEDOUT;
				}
			} else {
				if (0 == wait_for_completion_timeout(&upld_completion, msecs_to_jiffies(1000))) {
					loopback_log("wait to read timeout 1s");
					return -ETIMEDOUT;
				}
			}
		} else if (is_irq == 2) {
			loopback_log("can't support now");
		} else {
			loopback_log("polling card status");
			ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
			if (ret) {
				loopback_log("card is not rdy!");
				return -ENODEV;
			}
		}
	} else {
		start_flag = 1;
		loopback_log("first polling card status");
		ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
		if (ret) {
			loopback_log("card is not rdy!");
			start_flag = 0;
			return -ENODEV;
		}
	}

	/* step 2: read bitmap to know which ports are for opeartion */
	if (write)
		bitmap_addr = WR_BITMAP_L;
	else
		bitmap_addr = RD_BITMAP_L;

	ret = asr_sdio_readw(func, bitmap_addr, &bitmap);
	if (ret) {
		loopback_log("read bit map failed");
		return -ENODEV;
	}

	if (write) {// write
		// [2byte length][data][padding] should in block size
		aggr_wr_size = 0;
		port_used = 0;
		loopback_log("aggr_wr_size = 0");
		for (i = 0; i < 16; i++) {
			if (bitmap & (1 << i)) {
				aggr_wr_size += aggr_port_size[port_used];
				loopback_log(" + %d", aggr_port_size[port_used]);
				port_used++;
			}
		}
		loopback_log(";");
	} else {// read
		aggr_rd_size = 0;
		port_used = 0;
		for (i = 0; i < 16; i++) {
			aggr_port_size[i] = 0;
		}
		loopback_log("aggr_rd_size = 0");

		for (i = 0; i < 16; i++) {
			if (bitmap & (1 << i)) {
				ret = asr_sdio_readw(func, RD_LEN_L(i), &port_size);
				if (ret || (0 == port_size)) {
					loopback_log("aggr port read RD_LEN failed");
					return -ENODEV;
				}

				loopback_log(" + %d (from [%d]:0x%x 0x%x)", port_size, i, RD_LEN_L(i), RD_LEN_U(i));

				//alloc buffer
				aggr_port_buf[port_used] = kzalloc((size_t)port_size, GFP_KERNEL);
				if (!aggr_port_buf[port_used]) {
					for (j = 0; j < port_used; j++) {
						if (aggr_port_buf[j])
							kfree(aggr_port_buf[j]);
					}
					loopback_log("alloc memory failed during aggr-port test!, cycle: %d", i);
					return -ENOMEM;
				}

				// record each port size
				aggr_port_size[port_used] = port_size;
				port_used++;
				aggr_rd_size += port_size;
			}
		}
	}

	// found the bitmap area of aggr mode
	start = ffs(bitmap);
	end = fls(bitmap);
	// assume that there is full '1' between ffs and fls, take no care other situations now
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
					  end        start

		2nd situation
	>fls - 1<                      >ffs - 1<
		 |                              |
		 --------------------------------
		 |/////|                    |///|
		 |/////|                    |///|
		 |/////|                    |///|
		 --------------------------------
			   ^                    ^
			   |                    |
			 start                end
	 */

	// 2nd situation
	if (end - start == 15) {
		int zero_start, zero_end;
		zero_start = ffs(~bitmap & 0xFFFF);
		zero_end = fls(~bitmap & 0xFFFF);
		start_port = zero_end;
		agg_num = (16 - (zero_end - zero_start + 1));
		agg_bitmap = ((1 << agg_num) - 1) & 0xFF;
		addr = ioport | (1 << 12) | (agg_bitmap << 4) | start_port;
	} else {// 1st situation
		start_port = start - 1;
		agg_num = (end - start + 1);
		agg_bitmap = ((1 << agg_num) - 1) & 0xFF;
		addr = ioport | (1 << 12) | (agg_bitmap << 4) | start_port;
	}

	/* step 3: do read or write operation */
	loopback_log("CMD53 %s start port:%d (bitmap:0x%x) port count:%d agg_bitmap:0x%x addr:0x%x buf_size:%d",
					write ? "W ->" : "R <-", start_port, bitmap, agg_num, agg_bitmap, addr, write ? aggr_wr_size : aggr_rd_size);

	sdio_claim_host(func);
	if (write)
		ret = asr_sdio_rw_extended_sg(func, 1, addr, 0, aggr_port_buf, aggr_port_size, port_used, aggr_wr_size);
	else
		ret = asr_sdio_rw_extended_sg(func, 0, addr, 0, aggr_port_buf, aggr_port_size, port_used, aggr_rd_size);
	sdio_release_host(func);
	if (ret) {
		loopback_log("%s data failed", write ? "write" : "read");
		return ret;
	}

	loopback_log("%s Done", write ? "W" : "R");

	if (write) {
		for (i = 0; i < 16; i++) {
			if (aggr_port_buf[i]) {
				kfree(aggr_port_buf[i]);
				aggr_port_buf[i] = NULL;
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
	int block_size_arr[] = {512, 128, 32};
	int loop, test_times, ret, i;
	int aggr_block_size;

	loopback_log("start aggr port test...");

	reinit_completion(&upld_completion);
	reinit_completion(&dnld_completion);

	for (loop = 0; loop < ARRAY_SIZE(block_size_arr); loop++) {
		sdio_claim_host(func);
		aggr_block_size = block_size_arr[loop];
		ret = sdio_set_block_size(func, aggr_block_size);
		sdio_release_host(func);
		if (ret) {
			loopback_log("set block size %d failed", aggr_block_size);
			goto exit;
		}

		for (test_times = 0; test_times < 16; test_times++) {
			loopback_log("aggr port test, block size: %d, cycle: %d", aggr_block_size, test_times);
			if (loopback_rw_sg)
				ret = read_write_agg_port_test_sg(func, aggr_block_size, 0);
			else
				ret = read_write_agg_port_test(func, aggr_block_size, 0);
			if (ret) {
				loopback_log("aggr port test read failed");
				goto exit;
			}
			if (loopback_rw_sg)
				ret = read_write_agg_port_test_sg(func, aggr_block_size, 1);
			else
				ret = read_write_agg_port_test(func, aggr_block_size, 1);
			if (ret) {
				loopback_log("aggr port test write failed");
				goto exit;
			}
		}
	}

	pr_info("*******************************************\r\n"
			"******* aggregation port test done! *******\r\n"
			"*******************************************\r\n");

	return 0;

exit:
	if (aggr_rd_buf) {
		kfree(aggr_rd_buf);
		aggr_rd_buf = NULL;
	}

	if (loopback_rw_sg) {
		for (i = 0; i < 16; i++) {
			if (aggr_port_buf[i]) {
				kfree(aggr_port_buf[i]);
				aggr_port_buf[i] = NULL;
			}
		}
	}
	return ret;
}

/*
	sdio_loopback_test: execute loopback test case
*/
static int sdio_loopback_test(struct sdio_func *func)
{
	int ret;
	int i = 0;

	trigger_slaver_do_rxtest(func);

	while (i < loopback_times) {
		pr_info("####################### loopback_times: %d, i %d\n", loopback_times, i);
		/* single port test */
		if (loopback_test_mode & 0x1) {
			ret = single_port_tests(func);
			if (ret < 0)
				return ret;
		}
		/* multi port test */
		if (loopback_test_mode & 0x2) {
			ret = multi_port_tests(func);
			if (ret < 0)
				return ret;
		}
		/* aggregation mode test */
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
	throughput_write_test: do throughput write test
*/
static int throughput_write_read_test(struct sdio_func *func, bool write)
{
	int i, ret;
	uint32_t addr;
	u16 port_size, bitmap;
	int throughput_wr_size = 0;
	int throughput_rd_size = 0;

	if (start_flag == 0){
		start_flag = 1;
		//loopback_log("first Polling SDIO card status.");
		ret = poll_card_status(func, (write ? C2H_DNLD_CARD_RDY : C2H_UPLD_CARD_RDY) | C2H_IO_RDY);
		if (ret) {
			loopback_log("card is not rdy!");
			start_flag = 0;
			goto err;
		}
	}

	sdio_claim_host(func);
	ret = sdio_readsb(func, sdio_regs, 0, SDIO_REG_READ_LENGTH);
	sdio_release_host(func);
	if (ret) {
		loopback_log("read sdio regs failed");
		goto err;
	}

	if (write)
		bitmap = sdio_regs[WR_BITMAP_L] | sdio_regs[WR_BITMAP_U] << 8;
	else
		bitmap = sdio_regs[RD_BITMAP_L] | sdio_regs[RD_BITMAP_U] << 8;
	addr = 0x10000 | (1 << 12) | (bitmap << 4) | 0;

	if (write) {
		throughput_wr_size = throughput_packet_size * 8;
	}
	else {
		for (i = 0; i < 8; i++) {
			if (bitmap & (1 << i)) {
				port_size = sdio_regs[RD_LEN_L(i)] | sdio_regs[RD_LEN_U(i)] << 8;
				throughput_rd_size += port_size;
			}
		}
	}

	sdio_claim_host(func);
	ret = (write ? sdio_writesb(func, addr, throughput_buf, throughput_wr_size) : sdio_readsb(func, throughput_buf, addr, throughput_rd_size));
	sdio_release_host(func);
	if (ret) {
		loopback_log("%s data failed\n", write ? "write" : "read");
		goto err;
	}

	return ret;

err:
	kfree(throughput_buf);
	throughput_buf = NULL;

	kfree(sdio_regs);
	sdio_regs = NULL;

	return ret;
}

/*
	sdio_throughput_test: execute throughput test case
*/
static int sdio_throughput_test(struct sdio_func *func)
{
	int i, j, ret;

	reinit_completion(&upld_completion);
	reinit_completion(&dnld_completion);

	sdio_claim_host(func);
	ret = sdio_set_block_size(func, throughput_block_size);
	sdio_release_host(func);
	if (ret) {
		loopback_log("set block size %d failed", throughput_block_size);
		return ret;
	}

	throughput_buf = kzalloc((size_t)(throughput_packet_size * 8), GFP_KERNEL);
	if (!throughput_buf) {
		loopback_log("can't alloc buffer for throughput test");
		return -ENOMEM;
	}

	sdio_regs = kzalloc((size_t)SDIO_REG_READ_LENGTH, GFP_KERNEL);
	if (!sdio_regs) {
		loopback_log("can't alloc buffer for sdio_regs");
		goto exit;
	}

	for (i = 0; i < 8; i++) {
		for (j = 0; j < throughput_packet_size; j++) {
			((uint8_t *)throughput_buf)[i * throughput_packet_size + j] = (uint8_t)(j & 0xff);
		}

		/* set first 2 bytes as payload length */
		((uint16_t *)throughput_buf)[i * throughput_packet_size / 2] = throughput_packet_size;
	}

	loopback_log("start throughput write test, please wait...");
	start_jiffies = jiffies;
	ret = throughput_write_read_test(func, 1);
	if (ret) {
		loopback_log("throughput write failed");
		goto exit;
	}

	return 0;

exit:
	if (throughput_buf) {
		kfree(throughput_buf);
		throughput_buf = NULL;
		throughput_cnt = 0;
	}

	if (sdio_regs) {
		kfree(sdio_regs);
		sdio_regs = NULL;
	}

	return ret;
}

static void throughput_calculate(bool write)
{
	int diff_ms, throughput;

	stop_jiffies = jiffies;
	diff_ms = jiffies_to_msecs(stop_jiffies - start_jiffies);
	if (write) {
		throughput = (throughput_packet_size - 2) * 8 * 8 * 1000 / diff_ms * THROUGHPUT_TEST_CNT;
		pr_info("%s write throughput = %d bps\n", __func__, throughput);
	}
	else {
		throughput = throughput_packet_size * 8 * 8 * 1000 / diff_ms * THROUGHPUT_TEST_CNT;
		pr_info("%s read throughput = %d bps\n", __func__, throughput);
	}
}

/*
	asr_sdio_function_irq: isr in data1 if exist
*/
static void asr_sdio_function_irq(struct sdio_func *func)
{
	uint8_t reg_val;
	int ret = -1;

	sdio_claim_host(func);
	reg_val = sdio_readb(func, HOST_INT_STATUS, &ret);
	sdio_release_host(func);
	if (ret)
		return;

	sdio_claim_host(func);
	sdio_writeb(func, 0x0, HOST_INT_STATUS, &ret);
	sdio_release_host(func);
	if (ret)
		return;

	irq_times++;
	//pr_info("%s irq_times: %d enter ... int status: 0x%x\n", __func__, irq_times, reg_val);

	if (reg_val & HOST_INT_UPLD_ST) {
		mutex_lock(&upld_mutex);

		complete(&upld_completion);
		//pr_info("%s UPLD STATUS processed\n", __func__);

		if (is_throughput_test) {
			if (throughput_cnt < THROUGHPUT_TEST_CNT) {
				ret = throughput_write_read_test(func, 0);
				if (ret) {
					loopback_log("throughput read failed");
				}
			} else {
				throughput_calculate(0);

				kfree(throughput_buf);
				throughput_buf = NULL;
				kfree(sdio_regs);
				sdio_regs = NULL;
			}
			throughput_cnt++;
		}

		mutex_unlock(&upld_mutex);
	}

	if (reg_val & HOST_INT_DNLD_ST) {
		mutex_lock(&dnld_mutex);
		complete(&dnld_completion);
		//pr_info("%s DNLD STATUS processed\n", __func__);

		if (is_throughput_test) {
			throughput_cnt++;
			if (throughput_cnt < THROUGHPUT_TEST_CNT) {
				ret = throughput_write_read_test(func, 1);
				if (ret)  {
					loopback_log("throughput write failed");
				}
			} else {
				throughput_calculate(1);

				trigger_slaver_do_rxtest(func);
				throughput_cnt = 0;
				loopback_log("start throughput read test, please wait...");
				start_jiffies = jiffies;
			}
		}

		mutex_unlock(&dnld_mutex);
	}

	return;
}

static void asr_sdio_trigger_fw_init(struct sdio_func *func)
{
	int ret = 0;
	uint8_t reg_val = 0;

	/* Write Host_Pwr_Up bit to trigger card do initialization, used for combo SDIO3.0 */
	reg_val = (1 << 1);
	sdio_claim_host(func);
	sdio_writeb(func, reg_val, H2C_INTEVENT, &ret);
	sdio_release_host(func);

	msleep(500);

	pr_info("Send H2C_INTEVENT Host_Pwr_Up done, ret: %d\n", ret);

	return;
}

static int asr_sdio_loopback_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret = 0;

	if(func_number)
		func->num = func_number;

	pr_info("SDIO card num: %d, func num: %d\n", func->card->sdio_funcs, func->num);
	pr_info("class: %#X, vendor: %#X, device: %#X\n", func->class, func->vendor, func->device);

	sdio_claim_host(func);
	/* enable func */
	sdio_enable_func(func);
	/* set block size 512 */
	sdio_set_block_size(func, SDIO_BLOCK_SIZE_DLD);
	sdio_release_host(func);

	/*  every transfer will compare with func->cur_blksz, please check sdio_io_rw_ext_helper, it will decide
		which size use block mode and which size using byte mode by check: (size > sdio_max_byte_size(func)) */
	func->card->quirks |= MMC_QUIRK_BLKSZ_FOR_BYTE_MODE;

	/* step 0: get ioport */
	ioport = sdio_get_ioport(func);
	if (ioport < 0) {
		pr_info("%s get ioport failed\n", __func__);
		return -1;
	}

	if (fw_name) {
#ifdef CONFIG_ASR595X
		ret = download_firmware(func, "bootld_asr595x.bin");
		if (ret) {
			pr_info("%s download firmware failed ret: %d\n", __func__, ret);
			return ret;
		}
#endif

		ret = download_firmware(func, fw_name);
		if (ret) {
			pr_info("%s download firmware failed ret: %d\n", __func__, ret);
			return ret;
		}
	}

	mutex_init(&dnld_mutex);
	mutex_init(&upld_mutex);

	/*  enable host interrupt:
		is_irq = 0 no interrupts using polling way
		is_irq = 1 using data1 line as interrupt source
		is_irq = 2 not support
	*/
	sdio_claim_host(func);
	sdio_claim_irq(func, asr_sdio_function_irq);
	sdio_release_host(func);
	ret = asr_sdio_enable_int(func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);
	if (ret) {
		pr_info("%s asr_sdio_enable_int fail\n", __func__);
		return ret;
	}

	/* trigger card do init */
	asr_sdio_trigger_fw_init(func);

	/* after download firmware ready, run test */
	if (is_loopback_test) {
		ret = sdio_loopback_test(func);
		if (ret)
			goto exit;
	}

	if (is_throughput_test) {
		ret = sdio_throughput_test(func);
		if (ret)
			goto exit;
	}

	pr_info("%s done\n", __func__);

	return 0;

exit:
	asr_sdio_disable_int(func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);

	sdio_claim_host(func);
	sdio_release_irq(func);
	sdio_disable_func(func);
	sdio_release_host(func);

	return ret;
}

static void asr_sdio_loopback_remove(struct sdio_func *func)
{
	asr_sdio_disable_int(func, HOST_INT_DNLD_EN | HOST_INT_UPLD_EN);

	sdio_claim_host(func);
	sdio_release_irq(func);
	sdio_disable_func(func);
	sdio_release_host(func);

	pr_info("%s done\n", __func__);
}

static const struct sdio_device_id asr_sdio_loopback_ids[] = {
	// combo/lega
	{SDIO_DEVICE(0x424c, 0x6006)},
	{SDIO_DEVICE(0x02df, 0x912d)},
	// canon
	{SDIO_DEVICE(0x424c, 0x700a)},
	//{SDIO_DEVICE(0x424c, 0x700b)},
	// bass
	{SDIO_DEVICE(0x424c, 0x701a)},
	//{SDIO_DEVICE(0x424c, 0x701b)},
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

static int __init asr_sdio_init(void)
{
	pr_info("%s enter...\n", __func__);
	sdio_register_driver(&asr_sdio_loopback_driver);
	pr_info("%s leave\n", __func__);
	return 0;
}

static void __exit asr_sdio_exit(void)
{
	pr_info("%s enter...\n", __func__);
	sdio_unregister_driver(&asr_sdio_loopback_driver);
	pr_info("sdio unregister done\r\n");
	pr_info("%s leave\n", __func__);
}

module_init(asr_sdio_init);
module_exit(asr_sdio_exit);
module_param(fw_name, charp, 0660);
module_param(is_irq, int, 0660);
module_param(loopback_times, int, 0660);
module_param(loopback_test_mode, int, 0660);
module_param(is_loopback_test, bool, 0660);
module_param(func_number, int, 0660);
module_param(is_loopback_display, bool, 0660);
module_param(is_throughput_test, bool, 0660);
module_param(throughput_block_size, int, 0660);
module_param(throughput_packet_size, int, 0660);
module_param(loopback_rw_sg, bool, 0660);

MODULE_PARM_DESC(fw_name, "Specify the firmware name");
MODULE_AUTHOR("qipanli@asrmicro.com");
MODULE_DESCRIPTION("asr sdio sdio driver");
MODULE_LICENSE("GPL");

