/**
 ******************************************************************************
 *
 * @file asr_sdio.c
 *
 * Copyright (C) 2021 ASR Microelectronics Co., Ltd.
 * All Rights Reserved.
 *
 ******************************************************************************
 */
#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/ktime.h>
#include <linux/timer.h>
#include "asr_defs.h"
#include "asr_sdio.h"
#include "asr_hif.h"
#include "asr_utils.h"

static struct timer_list g_sdio_dev_detect_timer;

extern int asr_gpio_irq;
extern bool txlogen;

/*
get_ioport: get ioport value
*/
int sdio_get_ioport(struct sdio_func *func)
{
	int ret;
	u8 reg;
	int ioport;

	/*get out ioport value */
	sdio_claim_host(func);
	reg = sdio_readb(func, IO_PORT_0, &ret);
	sdio_release_host(func);
	if (ret) {
		dev_err(g_asr_para.dev, "%s can't read 0x%x\n", __func__, IO_PORT_0);
		goto exit;
	}
	ioport = reg & 0xff;
	sdio_claim_host(func);
	reg = sdio_readb(func, IO_PORT_1, &ret);
	sdio_release_host(func);
	if (ret) {
		dev_err(g_asr_para.dev, "%s can't read 0x%x\n", __func__, IO_PORT_1);
		goto exit;
	}
	ioport |= ((reg & 0xff) << 8);
	sdio_claim_host(func);
	reg = sdio_readb(func, IO_PORT_2, &ret);
	sdio_release_host(func);
	if (ret) {
		dev_err(g_asr_para.dev, "%s can't read 0x%x\n", __func__, IO_PORT_2);
		goto exit;
	}
	ioport |= ((reg & 0xff) << 16);

	return ioport;
exit:
	return -1;
}

int asr_sdio_config_rsr(struct sdio_func *func, u8 mask)
{
	u8 reg_val;
	int ret = 0;

	sdio_claim_host(func);
	reg_val = sdio_readb(func, HOST_INT_RSR, &ret);
	sdio_release_host(func);
	if (ret)
		return ret;

	reg_val |= mask;
	sdio_claim_host(func);
	sdio_writeb(func, reg_val, HOST_INT_RSR, &ret);
	sdio_release_host(func);

	return ret;
}

int asr_sdio_config_auto_reenable(struct sdio_func *func)
{
	u8 reg_val;
	int ret = 0;

	sdio_claim_host(func);
	reg_val = sdio_readb(func, SDIO_CONFIG2, &ret);
	sdio_release_host(func);
	if (ret)
		return ret;

	reg_val |= AUTO_RE_ENABLE_INT;

	sdio_claim_host(func);
	sdio_writeb(func, reg_val, SDIO_CONFIG2, &ret);
	sdio_release_host(func);

	return ret;
}

int asr_sdio_enable_int(struct sdio_func *func, int mask)
{
	u8 reg_val;
	int ret = -1;

	if (!(mask & HOST_INT_MASK)) {
		dev_err(g_asr_para.dev, "%s 0x%x is invalid int mask\n", __func__, mask);
		return ret;
	}

	/*enable specific interrupt */
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

void asr_sdio_disable_int(struct sdio_func *func, int mask)
{
	u8 reg_val;
	int ret = -1;

	/*enable specific interrupt */
	sdio_claim_host(func);
	reg_val = sdio_readb(func, HOST_INTMASK, &ret);
	sdio_release_host(func);
	if (ret) {
		dev_err(g_asr_para.dev, "%s read host int mask failed\n", __func__);
		return;
	}

	reg_val &= ~mask;
	sdio_claim_host(func);
	sdio_writeb(func, reg_val, HOST_INTMASK, &ret);
	sdio_release_host(func);
	if (ret) {
		dev_err(g_asr_para.dev, "%s disable host int mask failed\n", __func__);
	}
}

static u32 asr_crc32(const u8 * buf, u32 len)
{
	u32 i;
	u32 crc = 0xffffffff;

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
#if defined(CONFIG_ASR5505) || defined(CONFIG_ASR595X)
static u32 asr_recursive_crc32(u32 init_crc, const u8 * buf, u32 len)
{
    u32 i;
	u32 crc;

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
			dev_err(g_asr_para.dev, "%s can't read 0x%x out\n", __func__, C2H_INTEVENT);
			break;
		}
		dev_info(g_asr_para.dev, "%s reg_val:0x%x mask:0x%x\n", __func__, reg_val, mask);
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
		dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__, 4);
		return -1;
	}

	for (tries = 0; tries < MAX_POLL_TRIES; tries++) {
		sdio_claim_host(func);
		ret = sdio_readsb(func, scratch_val, SCRATCH_0, 4);
		sdio_release_host(func);
		if (ret) {
			dev_err(g_asr_para.dev, "%s can't read 0x%x out\n", __func__, SCRATCH_0);
			break;
		}

		dev_info(g_asr_para.dev, "%s scratch_val1:0x%x status:0x%x\n", __func__, *(u32*)scratch_val, status);
		if ((*(u32*)scratch_val & status) == status) {
			if (scratch_val) {
				devm_kfree(&func->dev, scratch_val);
				scratch_val = NULL;
			}
			return 0;	//success
		}
		msleep(100);
	}

	if (scratch_val) {
		devm_kfree(&func->dev, scratch_val);
		scratch_val = NULL;
	}

	return -1;		//fail
}

/*
    download_firmware: download firmware into card
    total data format will be: | header data | fw_data | padding | CRC |(CRC check fw_data + padding)
    header data will be: | fw_len | transfer unit | transfer times |
*/
#ifdef CONFIG_ASR5531
#define HYBRID_BIN_TAG 0xAABBCCDD
int asr_sdio_download_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	u8 *fw_buf;
	u8 *fw = NULL;
	u8 *tempbuf = NULL;
	int ret = -1;
	//struct asr_hw *asr_hw = sdio_get_drvdata(func);
	struct sdio_host_sec_fw_header sdio_host_sec_fw_header = { 0 };
	u32 total_sec_num, fw_tag, sec_idx, fw_hybrid_header_len, fw_sec_total_len, fw_dled_len, sec_total_len;
	u8 fw_sec_num;
	u32 jump_addr;

	/*parse fw header and get sec num */
	memcpy(&fw_tag, fw_img->data, sizeof(u32));

	if (HYBRID_BIN_TAG != fw_tag) {
		dev_err(g_asr_para.dev, "%s fw tag mismatch(0x%x 0x%x)\n", __func__, HYBRID_BIN_TAG, fw_tag);
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
		dev_err(g_asr_para.dev, "%s write fw_sec_num  fail!!! (%d)\n", __func__, ret);
		return -1;
	}
	// tag + sec num + [len/crc/addr]...
	fw_hybrid_header_len = 8 + total_sec_num * sizeof(struct sdio_fw_sec_fw_header);

	fw_sec_total_len = fw_img->size - fw_hybrid_header_len;
	if (fw_sec_total_len % 4) {
		dev_err(g_asr_para.dev, "%s error: fw bin length not 4byte allignment\n", __func__);
		return -1;
	}

	dev_err(g_asr_para.dev, "%s hybrid hdr (0x%x %d %d %d)!!\n", __func__, fw_tag,
		total_sec_num, fw_hybrid_header_len, fw_sec_total_len);

	sec_idx = 0;
	fw_dled_len = 0;
	do {
		// prepare sdio_host_sec_fw_header
		memcpy(&sdio_host_sec_fw_header,
		       fw_img->data + 8 +
		       sec_idx * sizeof(struct sdio_fw_sec_fw_header), sizeof(struct sdio_fw_sec_fw_header));

		if (sdio_host_sec_fw_header.sec_fw_len >= SDIO_BLOCK_SIZE_DLD * 511) {
			dev_err(g_asr_para.dev, "%s sec bin size(%d) too big !!\n", __func__,
				sdio_host_sec_fw_header.sec_fw_len);
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

		dev_info(g_asr_para.dev, "%s idx(%d),dled_len=%d,sec headers: 0x%x-0x%x-0x%x-0x%x-0x%x\n",
			 __func__, sec_idx, fw_dled_len,
			 sdio_host_sec_fw_header.sec_fw_len,
			 sdio_host_sec_fw_header.sec_crc,
			 sdio_host_sec_fw_header.chip_ram_addr,
			 sdio_host_sec_fw_header.transfer_unit, sdio_host_sec_fw_header.transfer_times);

		// sec bin total len.
		sec_total_len = sdio_host_sec_fw_header.sec_fw_len + sizeof(struct sdio_host_sec_fw_header);

		tempbuf = devm_kzalloc(&func->dev, sec_total_len + SDIO_BLOCK_SIZE_DLD, GFP_KERNEL | GFP_DMA);
		if (!tempbuf) {
			dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__,
				sec_total_len + SDIO_BLOCK_SIZE_DLD);
			goto exit;
		}
		fw_buf = (u8 *) (ALIGN_ADDR(tempbuf, DMA_ALIGNMENT));	//DMA aligned buf for sdio transmit

		fw = (u8 *) devm_kzalloc(&func->dev, sdio_host_sec_fw_header.sec_fw_len, GFP_KERNEL);
		if (!fw) {
			dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__,
				sdio_host_sec_fw_header.sec_fw_len);
			goto exit;
		}
		memcpy(fw, fw_img->data + fw_hybrid_header_len + fw_dled_len, sdio_host_sec_fw_header.sec_fw_len);

		if (asr_sdio_send_section_firmware(func, &sdio_host_sec_fw_header, fw_buf, fw)) {
			// any section fail,exit
			dev_err(g_asr_para.dev, "%s sec(%d) download fail!\n", __func__, sec_idx);
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
		dev_err(g_asr_para.dev, "%s fw len mismatch(%d %d)\n", __func__, fw_dled_len, fw_sec_total_len);
		return -1;
	}

	/*step 5 check fw runing status */
	if (check_scratch_status(func, BOOT_SUCCESS) < 0)
		goto exit;

	dev_info(g_asr_para.dev, "%s BOOT_SUCCESS:0x%x\n", __func__, BOOT_SUCCESS);

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

int asr_sdio_send_section_firmware(struct sdio_func *func, struct sdio_host_sec_fw_header
				   *p_sec_fw_hdr, u8 * fw_buf, u8 * fw)
{

	int retry_times = 3;
	int ret = -1;
	struct asr_hw *asr_hw = sdio_get_drvdata(func);

	do {
		/*step 1 polling if card is in rdy */
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
			dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
			break;
		}

		/*step 2 send out header data */
		memmove(fw_buf, p_sec_fw_hdr, sizeof(struct sdio_host_sec_fw_header));
		ret =
		    asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf,
						     sizeof(struct
							    sdio_host_sec_fw_header), (asr_hw->ioport | 0x0), 0x1);
		if (ret) {
			dev_err(g_asr_para.dev, "%s write header data failed %d\n", __func__, ret);
			break;
		}

		/*step 3 send out fw data */
		do {

			memmove(fw_buf, fw, p_sec_fw_hdr->sec_fw_len);
			//use msg port 0, still polling
			ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
			if (ret) {
				dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
				break;
			}

			ret =
			    asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf,
							     p_sec_fw_hdr->sec_fw_len, asr_hw->ioport | 0x0, 0x1);
			if (ret) {
				dev_err(g_asr_para.dev, "%s can't write %d data into card %d\n",
					__func__, p_sec_fw_hdr->sec_fw_len, ret);
				break;
			}
		} while (0);

		/*step 4 check fw_crc status */
		if (check_scratch_status(func, CRC_SUCCESS) == 0) {
			dev_info(g_asr_para.dev, "%s CRC_SUCCESS:0x%x\n", __func__, CRC_SUCCESS);
			/* reset the fw_crc status to check next section */
			sdio_claim_host(func);
			sdio_writeb(func, sdio_readb(func,SCRATCH_1,0)&(~(CRC_SUCCESS>>8)), SCRATCH_1, &ret);// clear bit14 
			sdio_release_host(func);
			if (ret) {
				dev_err(g_asr_para.dev, "%s reset fw_crc status fail!!! (%d)\n", __func__, ret);
			} else  {
				ret = 0;
				break;
			}
		}

		retry_times--;
		dev_info(g_asr_para.dev, "%s retry %d times\n", __func__, 3 - retry_times);

	} while (retry_times > 0);

	return ret;

}

#elif defined(CONFIG_ASR5505) || defined(CONFIG_ASR595X)
static int asr_sdio_send_fw(struct sdio_func *func, const struct firmware *fw_img, u8 *fw_buf,
    int blk_size, u32 total_len, u32 pad_len)
{
	int ret = 0;
	int i = 0;
	u8 padding_data[4] = { 0 };

	u32 temp_fw_crc = ~0xffffffff;//crc init value
	u32 offset = 0;
	struct asr_hw *asr_hw = sdio_get_drvdata(func);


	while (i < fw_img->size / blk_size ){
		//use msg port 0, still polling
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
		    dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
		    goto exit;
		}

		memcpy(fw_buf, fw_img->data+i*blk_size, blk_size);
		temp_fw_crc = asr_recursive_crc32(~temp_fw_crc, fw_buf, blk_size);
		ret = asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf, blk_size, (asr_hw->ioport | 0x0), 0x1);
		if (ret) {
		    dev_err(g_asr_para.dev, "%s can't write %d data into card %d\n", __func__, total_len, ret);
		    goto exit;
		}
			//dev_err(g_asr_para.dev, "send %d bytes in block mode [%d]", blk_size, i);
		#if 0
		if (blk_size >= 2 * sdio_get_block_size())
		    dbg(D_ERR, D_UWIFI_CTRL, "send %d bytes in block mode [%d]", blk_size, i);
		else
		    dbg(D_ERR, D_UWIFI_CTRL, "send %d bytes in byte mode [%d]", blk_size, i);
		#endif
		i++;
	}

	if (!(total_len - i*blk_size || pad_len)) {
		goto exit;
	}

	memset(fw_buf, 0, blk_size);
	offset = 0;
	dev_info(g_asr_para.dev,"%s:fw_img->size=%lu,len=%d",
	__func__, fw_img->size, total_len - i*blk_size);

	//use msg port 0, still polling
	ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
	if (ret) {
		dev_err(g_asr_para.dev,"%s card is not in rdy\n", __func__);
		goto exit;
	}

	if (fw_img->size - i*blk_size != 0) {
		memcpy(fw_buf, fw_img->data+i*blk_size, fw_img->size - i*blk_size);
		temp_fw_crc = asr_recursive_crc32(~temp_fw_crc, fw_buf, fw_img->size - i*blk_size);
		offset = fw_img->size - i*blk_size;
	}

	if (pad_len) {
		memcpy(fw_buf + offset, padding_data, pad_len);
		temp_fw_crc = asr_recursive_crc32(~temp_fw_crc, fw_buf + offset, pad_len);
		offset += pad_len;
	}
	dev_info(g_asr_para.dev, "%s crc:0x%x\n", __func__, temp_fw_crc);
	memcpy(fw_buf + offset, (u8 *)&temp_fw_crc, 4);

	ret = asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf, blk_size, (asr_hw->ioport | 0x0), 0x1);
	if (ret) {
		dev_err(g_asr_para.dev, "%s can't write %d data into card %d\n", __func__, total_len, ret);
		goto exit;
	}
	exit:

	return ret;
}
int asr_sdio_send_header_firmware(struct sdio_func *func, u32 *header_data, u32 header_len, const struct firmware *fw_img, int blk_size, u32 total_len, u32 pad_len)
{
	int ret = -1;
	struct asr_hw *asr_hw = sdio_get_drvdata(func);

	do {
		/*step 1 polling if card is in rdy */
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
			dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
			break;
		}
		dev_info(g_asr_para.dev,
			 "%s blk_size:%d total_len:%d pad_len:%d headers: 0x%x-0x%x-0x%x\n", __func__,
			 blk_size, total_len, pad_len, header_data[0], header_data[1], header_data[2]);
		/*step 2 send out header data */
		ret = asr_sdio_tx_common_port_dispatch(asr_hw, (u8 *)header_data, header_len, (asr_hw->ioport | 0x0), 0x1);
		if (ret) {
			dev_err(g_asr_para.dev, "%s write header data failed %d\n", __func__, ret);
			break;
		}
		/*step 3 send out fw data */
		ret = asr_sdio_send_fw(func, fw_img, (u8*)header_data, blk_size, total_len, pad_len);
		if (ret) {
			dev_err(g_asr_para.dev,"%s can't write %d data into card %d\n", __func__, total_len, ret);
			break;
		}
		/*step 4 check fw_crc status */
		if (check_scratch_status(func, CRC_SUCCESS) == 0) {
			dev_info(g_asr_para.dev, "%s CRC_SUCCESS:0x%x\n", __func__, CRC_SUCCESS);
			ret = 0;
		} else {
			ret = -1;
			break;
		}
		/*step 5 check fw runing status */
		if (check_scratch_status(func, BOOT_SUCCESS) == 0) {
			dev_info(g_asr_para.dev, "%s BOOT_SUCCESS:0x%x\n", __func__, BOOT_SUCCESS);
			ret = 0;
			break;
		} else {
			ret = -1;
		}

	} while (0);

	sdio_claim_host(func);                                                                                                                                                                                                                                                
	sdio_writeb(func, 0x0, SCRATCH_0, &ret);
	sdio_writeb(func, 0x0, SCRATCH_1, &ret);
	sdio_release_host(func);
	return ret;
}
int asr_sdio_download_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	u32 *header_data = NULL;
	int ret = 0;
	u32 fw_len, total_len, pad_len, crc_len;
	int blk_size;

	blk_size = SDIO_BLOCK_SIZE_DLD*2;// /2 to change to byte mode; *2 change to block mode

	header_data = devm_kzalloc(&func->dev, blk_size + SDIO_BLOCK_SIZE_DLD, GFP_KERNEL | GFP_DMA);
	if (!header_data) {
		dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__, total_len + SDIO_BLOCK_SIZE_DLD);
		ret = -1;
		return ret;
	}

	/*step 0 prepare header data and other initializations */
	fw_len = fw_img->size;
	pad_len = (fw_len % 4) ? (4 - (fw_len % 4)) : 0;
	crc_len = 4;
	total_len = fw_len + pad_len + crc_len;
	total_len = ASR_ALIGN_DLDBLKSZ_HI(total_len);
	header_data[0] = fw_img->size + pad_len; //actual length of fw
	header_data[1] = blk_size; //transfer unit
	header_data[2] = total_len / blk_size; // transfer times
	if (total_len % blk_size)
		header_data[2] += 1;

	ret = asr_sdio_send_header_firmware(func, header_data, 12, fw_img, blk_size, total_len, pad_len);

	if (header_data) {
		devm_kfree(&func->dev, header_data);
		header_data = NULL;
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
/*
    download_firmware: download firmware into card
    total data format will be: | header data | fw_data | padding | CRC |(CRC check fw_data + padding)
    header data will be: | fw_len | transfer unit | transfer times |
*/

static int asr_sdio_send_fw(struct sdio_func *func, uint8_t *fw_img_data, uint32_t fw_img_size,
    u8 *fw_buf, int blk_size, u32 total_len, u32 pad_len)
{
    int ret = 0;
    int i = 0;
    u8 padding_data[4] = { 0 };
	struct asr_hw *asr_hw = sdio_get_drvdata(func);

    while (i < total_len / blk_size ){
        //use msg port 0, still polling
        ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
        if (ret) {
            dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
            return ret;
        }

        memcpy(fw_buf, fw_img_data+i*blk_size, blk_size);
		ret = asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf, blk_size, (asr_hw->ioport | 0x0), 0x1);
        if (ret) {
            dev_err(g_asr_para.dev, "%s can't write %d data into card %d\n", __func__, total_len, ret);
            return ret;
        }
		//dev_err(g_asr_para.dev, "send %d bytes in block mode [%d]", blk_size, i);
        #if 0
        if (blk_size >= 2 * sdio_get_block_size())
            dbg(D_ERR, D_UWIFI_CTRL, "send %d bytes in block mode [%d]", blk_size, i);
        else
            dbg(D_ERR, D_UWIFI_CTRL, "send %d bytes in byte mode [%d]", blk_size, i);
        #endif
        i++;
    }

    if (!(total_len - i*blk_size || pad_len)) {
        return ret;
    }

    memset(fw_buf, 0, blk_size);

    dev_info(g_asr_para.dev,"%s:fw_img_size=%d,len=%d",
        __func__, fw_img_size, total_len - i*blk_size);

    //use msg port 0, still polling
    ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
    if (ret) {
        dev_err(g_asr_para.dev,"%s card is not in rdy\n", __func__);
        return ret;
    }

    if (fw_img_size - i*blk_size != 0) {
        memcpy(fw_buf, fw_img_data+i*blk_size, fw_img_size - i*blk_size);
    }

    if (pad_len) {
        memcpy(fw_buf + fw_img_size - i*blk_size, padding_data, pad_len);
    }

	ret = asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf, blk_size, (asr_hw->ioport | 0x0), 0x1);
    if (ret) {
        dev_err(g_asr_para.dev, "%s can't write %d data into card %d\n", __func__, total_len, ret);
        return ret;
    }

    return ret;
}
/*
    Download fw README
    1, lega/duet chip integrate romcode v1.0, which support download directly into one continuous "ram" region
    2, canon chip intergrate romcode v2.0, which support download different sections of firmware into non-continuous "ram" region

    Now duet with wifi + ble function's firmware have different sections, and its "ram" region is non-continuous,
    but it integrated the romcode v1.0. so for duet support non-continuous "ram" region,
    it needs using 1st_firmware download the snd_bootloader, then using 2nd_firmware download work with snd_bootloader
    to download the total firmware with different section into different "ram" region.

*/
/*
    download_firmware: download firmware into card
    total data format will be: | header data | fw_data | padding | CRC |(CRC check fw_data + padding)
    header data will be: | fw_len | transfer unit | transfer times |
*/
int asr_sdio_download_1st_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	int ret = 0;
	u32 header_data[3];
	u32 fw_crc;
	u32 fw_len, total_len, pad_len, crc_len;
	u8 *fw_buf;
	u8 *fw = NULL;
	u8 *tempbuf = NULL;
	//u32 tempsz;
	u8 padding_data[4] = { 0 };
	u8 *temp_fw_crc = NULL;
	int retry_times = 3;
	struct asr_hw *asr_hw = sdio_get_drvdata(func);

	/*step 0 prepare header data and other initializations */
	fw_len = ASR_SND_BOOT_LOADER_SIZE;
	//pad_len = (fw_len % 4) ? (4 - (fw_len % 4)) : 0;
	pad_len = 0;
	crc_len = 4;
	total_len = fw_len + pad_len + crc_len;
	total_len = ASR_ALIGN_DLDBLKSZ_HI(total_len);
	header_data[0] = ASR_SND_BOOT_LOADER_SIZE + pad_len;
	header_data[1] = total_len;
	header_data[2] = 1;
	//tempsz = ALIGN_SZ(WLAN_UPLD_SIZE, DMA_ALIGNMENT);
	tempbuf = devm_kzalloc(&func->dev, total_len + SDIO_BLOCK_SIZE_DLD, GFP_KERNEL | GFP_DMA);
	if (!tempbuf) {
		dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__, total_len + SDIO_BLOCK_SIZE_DLD);
		ret = -1;
		goto exit;
	}
	fw_buf = (u8 *) (ALIGN_ADDR(tempbuf, DMA_ALIGNMENT));
	/*collect padding and crc data into one place */
	fw = (u8 *) devm_kzalloc(&func->dev, total_len, GFP_KERNEL);
	if (!fw) {
		dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__, total_len);
		ret = -1;
		goto exit;
	}
	temp_fw_crc = devm_kzalloc(&func->dev, crc_len, GFP_KERNEL);
	if (!temp_fw_crc) {
		ret = -1;
		goto exit;
	}
//retry:
	if (!retry_times) {
		ret = -1;
		goto exit;
	}
	dev_info(g_asr_para.dev, "%s retry %d times\n", __func__, 3 - retry_times);
	retry_times--;
	{
		memmove(fw_buf, (u8 *) header_data, sizeof(header_data));
		memcpy(fw, fw_img->data, ASR_SND_BOOT_LOADER_SIZE);
		memcpy(fw + ASR_SND_BOOT_LOADER_SIZE, padding_data, pad_len);
		/*calculate CRC, data parts include the padding data */
		fw_crc = asr_crc32(fw, ASR_SND_BOOT_LOADER_SIZE + pad_len);

		*(u32 *) (temp_fw_crc) = fw_crc;
		memcpy(fw + ASR_SND_BOOT_LOADER_SIZE + pad_len, (u8 *) temp_fw_crc, crc_len);

		dev_info(asr_hw->dev,
			 "%s fw_len:%d pad_len:%d crc_len:%d total_len:%d headers: 0x%x-0x%x-0x%x crc:0x%x\n", __func__,
			 fw_len, pad_len, crc_len, total_len, header_data[0], header_data[1], header_data[2], fw_crc);
	}

	/*step 1 polling if card is in rdy */
	ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
	if (ret) {
		dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
		ret = -1;
		goto exit;
	}

	/*step 2 send out header data */
	ret = asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf, sizeof(header_data), (asr_hw->ioport | 0x0), 0x1);
	if (ret) {
		dev_err(g_asr_para.dev, "%s write header data failed %d\n", __func__, ret);
		ret = -1;
		goto exit;
	}

	/*step 3 send out fw data */
	do {
		memmove(fw_buf, fw, total_len);

		//use msg port 0, still polling
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
			dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
			ret = -1;
			goto exit;
		}

		ret = asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf, total_len, asr_hw->ioport | 0x0, 0x1);
		if (ret) {
			dev_err(g_asr_para.dev, "%s can't write %d data into card %d\n", __func__, total_len, ret);
			ret = -1;
			goto exit;
		}
	} while (0);

	/*step 4 check fw_crc status */
	//if (check_scratch_status(func, CRC_SUCCESS) < 0)
	//    goto retry;
	//dev_info(g_asr_para.dev, "%s CRC_SUCCESS:0x%x\n", __func__, CRC_SUCCESS);

	/*step 5 check fw runing status */
	if (check_scratch_status(func, BOOT_SUCCESS) < 0) {
		ret = -1;
		goto exit;
	}
	dev_info(g_asr_para.dev, "%s BOOT_SUCCESS:0x%x\n", __func__, BOOT_SUCCESS);

	ret = 0;

exit:
	if (tempbuf) {
		devm_kfree(&func->dev, tempbuf);
		tempbuf = NULL;
	}
	if (fw) {
		devm_kfree(&func->dev, fw);
		tempbuf = NULL;
	}
	if (temp_fw_crc) {
		devm_kfree(&func->dev, temp_fw_crc);
		tempbuf = NULL;
	}

	return ret;
}
#if 0
static int asr_sdio_clear_status(struct sdio_func *func, uint16_t status)
{
    int ret = 0;
    uint8_t scratch_val;

	sdio_claim_host(func);
    scratch_val = sdio_readb(func, SCRATCH_1, &ret);
	//ret = sdio_readsb(func, &scratch_val, SCRATCH_0, 4);
	sdio_release_host(func);
    if (!ret) {
        dev_err(g_asr_para.dev, "can't read 0x%x out\n",  SCRATCH_1);
        return -1;
    }
	sdio_claim_host(func);
    sdio_writeb(func, scratch_val | ~(status >> 8), SCRATCH_1, &ret);
	sdio_release_host(func);
    //asr_sdio_release_host();
    if (!ret) {
        dev_err(g_asr_para.dev, "%s write fw_sec_num  fail!!! (%d)\n", __func__, ret);
        return -1;
    }
    dev_info(g_asr_para.dev,"%s clear 0x%X", __func__, status);

    return 0;
}
#endif
#define HYBRID_BIN_TAG 0xAABBCCDD

int asr_sdio_send_section_firmware(struct sdio_func *func, struct sdio_host_sec_fw_header
				   *p_sec_fw_hdr, u8 * fw_buf, u8 * fw, int blk_size)
{

	int retry_times = 3;
	int ret = -1;
	struct asr_hw *asr_hw = sdio_get_drvdata(func);

	do {
		/*step 1 polling if card is in rdy */
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
			dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
			break;
		}

		/*step 2 send out header data */
		memmove(fw_buf, p_sec_fw_hdr, sizeof(struct sdio_host_sec_fw_header));
		ret = asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf, sizeof(struct sdio_host_sec_fw_header), (asr_hw->ioport | 0x0), 0x1);
		if (ret) {
			dev_err(g_asr_para.dev, "%s write header data failed %d\n", __func__, ret);
			break;
		}

		/*step 3 send out fw data */
		ret = asr_sdio_send_fw(func, fw, p_sec_fw_hdr->sec_fw_len, fw_buf, blk_size, p_sec_fw_hdr->sec_fw_len, 0);
		if (ret) {
			dev_err(g_asr_para.dev,"%s can't write %d data into card %d\n", __func__, p_sec_fw_hdr->sec_fw_len, ret);
			return ret;
		}


		/*step 4 check fw_crc status */
		if (check_scratch_status(func, CRC_SUCCESS) == 0) {
			dev_info(g_asr_para.dev, "%s CRC_SUCCESS:0x%x\n", __func__, CRC_SUCCESS);
			ret = 0;
			break;
		}

		retry_times--;
		dev_info(g_asr_para.dev, "%s retry %d times\n", __func__, 3 - retry_times);

	} while (retry_times > 0);

	return ret;

}

int asr_sdio_download_2nd_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	u8 *fw_buf;
	u8 *fw = NULL;
	u8 *tempbuf = NULL;
	const u8 *fw_data;
	size_t fw_size;
	int ret = -1;
	struct asr_hw *asr_hw = sdio_get_drvdata(func);
	struct sdio_host_sec_fw_header sdio_host_sec_fw_header = { 0 };
	u32 total_sec_num, fw_tag, sec_idx, fw_hybrid_header_len, fw_sec_total_len, fw_dled_len, sec_total_len;
	u8 fw_sec_num;
	int blk_size;

    blk_size = SDIO_BLOCK_SIZE_DLD*2;// /2 to change to byte mode; *2 change to block mode
	/*parse fw header and get sec num */
	fw_data = fw_img->data + ASR_SND_BOOT_LOADER_SIZE;
	fw_size = fw_img->size - ASR_SND_BOOT_LOADER_SIZE;
	memcpy(&fw_tag, fw_data, sizeof(u32));

	if (HYBRID_BIN_TAG != fw_tag) {
		dev_err(g_asr_para.dev, "%s fw tag mismatch(0x%x 0x%x)\n", __func__, HYBRID_BIN_TAG, fw_tag);
		ret = -1;
		goto exit;
	}
	memcpy(&total_sec_num, fw_data + 4, sizeof(u32));
	fw_sec_num = (u8) total_sec_num;
	// write sec num to scra register for fw read.
	sdio_claim_host(func);
	sdio_writeb(func, fw_sec_num, SCRATCH0_3, &ret);
	sdio_writeb(func, asr_hw->mac_addr[0], SCRATCH1_0, &ret);
	sdio_writeb(func, asr_hw->mac_addr[1], SCRATCH1_1, &ret);
	sdio_writeb(func, asr_hw->mac_addr[2], SCRATCH1_2, &ret);
	sdio_writeb(func, asr_hw->mac_addr[5], SCRATCH1_3, &ret);
	sdio_release_host(func);
	if (ret) {
		dev_err(g_asr_para.dev, "%s write fw_sec_num  fail!!! (%d)\n", __func__, ret);
		ret = -1;
		goto exit;
	}
	// tag + sec num + [len/crc/addr]...+
	fw_hybrid_header_len = 8 + total_sec_num * sizeof(struct sdio_fw_sec_fw_header);
	fw_sec_total_len = fw_size - fw_hybrid_header_len;
	if (fw_sec_total_len % 4) {
		dev_err(g_asr_para.dev, "%s error: fw bin length not 4byte allignment\n", __func__);
		ret = -1;
		goto exit;
	}
	dev_err(g_asr_para.dev, "%s hybrid hdr (0x%x %d %d %d)!!\n", __func__, fw_tag,
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
		dev_info(asr_hw->dev, "%s idx(%d),dled_len=%d,sec headers: 0x%x-0x%x-0x%x-0x%x-0x%x\n",
			 __func__, sec_idx, fw_dled_len,
			 sdio_host_sec_fw_header.sec_fw_len,
			 sdio_host_sec_fw_header.sec_crc,
			 sdio_host_sec_fw_header.chip_ram_addr,
			 sdio_host_sec_fw_header.transfer_unit, sdio_host_sec_fw_header.transfer_times);

		// sec bin total len.
		sec_total_len = sdio_host_sec_fw_header.sec_fw_len + sizeof(struct sdio_host_sec_fw_header);

		tempbuf = devm_kzalloc(&func->dev, blk_size*2, GFP_KERNEL | GFP_DMA);
		if (!tempbuf) {
			dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__,
				sec_total_len + SDIO_BLOCK_SIZE_DLD);
			ret = -1;
			goto exit;
		}
		fw_buf = (u8 *) (ALIGN_ADDR(tempbuf, DMA_ALIGNMENT));	//DMA aligned buf for sdio transmit
		#if 0
		fw = (u8 *) devm_kzalloc(&func->dev, sdio_host_sec_fw_header.sec_fw_len, GFP_KERNEL);
		if (!fw) {
			dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__,
				sdio_host_sec_fw_header.sec_fw_len);
			ret = -1;
			goto exit;
		}
		memcpy(fw, fw_data + fw_hybrid_header_len + fw_dled_len, sdio_host_sec_fw_header.sec_fw_len);
		#endif
		if (asr_sdio_send_section_firmware(func, &sdio_host_sec_fw_header, fw_buf, (u8 *)(fw_data + fw_hybrid_header_len + fw_dled_len), blk_size)) {
			// any section fail,exit
			dev_err(g_asr_para.dev, "%s sec(%d) download fail!\n", __func__, sec_idx);
			ret = -1;
			goto exit;
		}
		// download success, free buf
		if (tempbuf) {
			devm_kfree(&func->dev, tempbuf);
			tempbuf = NULL;
		}
		#if 0
		if (fw) {
			devm_kfree(&func->dev, fw);
			fw = NULL;
		}
		#endif
		// caculate downloaded bin length.
		fw_dled_len += sdio_host_sec_fw_header.sec_fw_len;
		sec_idx++;
	} while (sec_idx < total_sec_num);

	if (fw_dled_len != fw_sec_total_len) {
		dev_err(g_asr_para.dev, "%s fw len mismatch(%d %d)\n", __func__, fw_dled_len, fw_sec_total_len);
		ret = -1;
		goto exit;
	}
	/*step 5 check fw runing status */
	if (check_scratch_status(func, BOOT_SUCCESS) < 0) {
		ret = -1;
		goto exit;
	}

	msleep(20);
	dev_info(g_asr_para.dev, "%s BOOT_SUCCESS:0x%x\n", __func__, BOOT_SUCCESS);

	if (poll_card_status(asr_hw->plat->func, C2H_IO_RDY)) {
		dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
		ret = -1;
		goto exit;
	}

	ret = 0;

exit:
	if (tempbuf) {
		devm_kfree(&func->dev, tempbuf);
		tempbuf = NULL;
	}
	if (fw) {
		devm_kfree(&func->dev, fw);
		fw = NULL;
	}

	return ret;

}

#endif

int asr_sdio_download_ATE_firmware(struct sdio_func *func, const struct firmware *fw_img)
{
	int ret;
	u32 header_data[3];
	u32 fw_crc;
	u32 fw_len, total_len, pad_len, crc_len;
	u8 *fw_buf;
	u8 *fw = NULL;
	u8 *tempbuf = NULL;
	//u32 tempsz;
	u8 padding_data[4] = { 0 };
	u8 *temp_fw_crc = NULL;
	struct asr_hw *asr_hw = sdio_get_drvdata(func);

	/*step 0 prepare header data and other initializations */
	fw_len = fw_img->size;
	pad_len = (fw_len % 4) ? (4 - (fw_len % 4)) : 0;
	crc_len = 4;
	total_len = fw_len + pad_len + crc_len;
	total_len = ASR_ALIGN_DLDBLKSZ_HI(total_len);
	header_data[0] = fw_len + pad_len;
	header_data[1] = total_len;
	header_data[2] = 1;
	//tempsz = ALIGN_SZ(WLAN_UPLD_SIZE, DMA_ALIGNMENT);
	tempbuf = devm_kzalloc(&func->dev, total_len + SDIO_BLOCK_SIZE_DLD, GFP_KERNEL | GFP_DMA);
	if (!tempbuf) {
		dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__, total_len + SDIO_BLOCK_SIZE_DLD);
		goto exit;
	}
	fw_buf = (u8 *) (ALIGN_ADDR(tempbuf, DMA_ALIGNMENT));
	/*collect padding and crc data into one place */
	fw = (u8 *) devm_kzalloc(&func->dev, total_len, GFP_KERNEL);
	if (!fw) {
		dev_err(g_asr_para.dev, "%s can't alloc %d memory\n", __func__, total_len);
		goto exit;
	}
	temp_fw_crc = devm_kzalloc(&func->dev, crc_len, GFP_KERNEL);
	if (!temp_fw_crc) {
		goto exit;
	}

	memmove(fw_buf, (u8 *) header_data, sizeof(header_data));
	memcpy(fw, fw_img->data, fw_len);
	memcpy(fw + fw_len, padding_data, pad_len);
	/*calculate CRC, data parts include the padding data */
	fw_crc = asr_crc32(fw, fw_len + pad_len);

	*(u32 *) (temp_fw_crc) = fw_crc;
	memcpy(fw + fw_len + pad_len, (u8 *) temp_fw_crc, crc_len);

	dev_info(asr_hw->dev, "%s fw_len:%d pad_len:%d crc_len:%d total_len:%d headers: 0x%x-0x%x-0x%x crc:0x%x\n",
		 __func__, fw_len, pad_len, crc_len, total_len, header_data[0], header_data[1], header_data[2], fw_crc);

	/*step 1 polling if card is in rdy */
	ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
	if (ret) {
		dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
		goto exit;
	}

	/*step 2 send out header data */
	ret = asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf, sizeof(header_data), (asr_hw->ioport | 0x0), 0x1);
	if (ret) {
		dev_err(g_asr_para.dev, "%s write header data failed %d\n", __func__, ret);
		goto exit;
	}

	/*step 3 send out fw data */
	do {
		memmove(fw_buf, fw, total_len);

		//use msg port 0, still polling
		ret = poll_card_status(func, C2H_DNLD_CARD_RDY | C2H_IO_RDY);
		if (ret) {
			dev_err(g_asr_para.dev, "%s card is not in rdy\n", __func__);
			goto exit;
		}

		ret = asr_sdio_tx_common_port_dispatch(asr_hw, fw_buf, total_len, asr_hw->ioport | 0x0, 0x1);
		if (ret) {
			dev_err(g_asr_para.dev, "%s can't write %d data into card %d\n", __func__, total_len, ret);
			goto exit;
		}
	} while (0);

	/*step 5 check fw runing status */
	dev_info(g_asr_para.dev, "%s BOOT_SUCCESS:0x%x\n", __func__, BOOT_SUCCESS);

	return 0;
exit:
	if (tempbuf) {
		devm_kfree(&func->dev, tempbuf);
		tempbuf = NULL;
	}
	if (fw) {
		devm_kfree(&func->dev, fw);
		tempbuf = NULL;
	}
	if (temp_fw_crc) {
		devm_kfree(&func->dev, temp_fw_crc);
		tempbuf = NULL;
	}

	return -1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static void sdio_dev_detect_timer_callback(struct timer_list *timer)
#else
static void sdio_dev_detect_timer_callback(unsigned long timer)
#endif
{
	dev_err(g_asr_para.dev, "ASR: ERROR detect sdio device fail.\n");
	dev_err(g_asr_para.dev, "ASR: Could not read interface wlan0 flags: No such device.\n");
}

static int asr_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	struct asr_plat *asr_plat = NULL;
	void *drvdata;
	int ret = -ENODEV;

	ASR_DBG(ASR_FN_ENTRY_STR);
	asr_dbg(SDIO, "asr_sdio_probe func num = %d\n", func->num);

	del_timer(&g_sdio_dev_detect_timer);
	dev_info(&func->dev, "ASR: %s detect sdio device(%s) success.\n", __func__, mmc_hostname(func->card->host));
	dev_info(&func->dev, "%s: %s ios: %d hz, %d bit width\n",
		 __func__, mmc_hostname(func->card->host), func->card->host->ios.clock,
		 1 << func->card->host->ios.bus_width);

	sdio_claim_host(func);
	/*enable func */
	sdio_enable_func(func);
	/*set block size SDIO_BLOCK_SIZE */
	sdio_set_block_size(func, SDIO_BLOCK_SIZE_DLD);
	sdio_release_host(func);
	/*every transfer will compare with func->cur_blksz, please check sdio_io_rw_ext_helper, it will decide
	   which size use block mode and which size using byte mode by check:
	   (size > sdio_max_byte_size(func)) */
	func->card->quirks |= MMC_QUIRK_BLKSZ_FOR_BYTE_MODE;

	if (g_asr_para.dev_reset_start && g_asr_para.asr_hw) {
		g_asr_para.dev_reset_start = false;
		dev_err(&func->dev, "ASR: send dev reset event.\n");

		g_asr_para.asr_hw->plat->func = func;

		complete(&g_asr_para.reset_complete);
		return 0;
	}

	asr_plat = kzalloc(sizeof(struct asr_plat), GFP_KERNEL);
	if (!asr_plat)
		return -ENOMEM;

	asr_plat->func = func;
	//asr_plat->irq = asr_gpio_irq;

	ret = asr_platform_init(asr_plat, &drvdata);
	if (ret)
		kfree(asr_plat);

	return ret;
}

static void asr_sdio_remove(struct sdio_func *func)
{
	struct asr_hw *asr_hw;

	ASR_DBG(ASR_FN_ENTRY_STR);

	dev_err(&func->dev, "%s: remove.\n", __func__);

	asr_hw = sdio_get_drvdata(func);
	if (asr_hw == NULL) {
		return;
	}

	if (g_asr_para.dev_driver_remove || !g_asr_para.dev_reset_start) {

		asr_platform_deinit(asr_hw);
		kfree(asr_hw->plat);
	} else {
		asr_hw->plat->func = NULL;
	}

	sdio_set_drvdata(func, NULL);

	sdio_claim_host(func);
	sdio_disable_func(func);
	sdio_release_host(func);

}

/** Device ID for SD5531 */
#define SD_DEVICE_ID_5531_FN0     0x600a
#define SD_DEVICE_ID_5531_FN1     0x700a
#define SD_DEVICE_ID_5531_BT_FN2  0x700b

/** define asr vendor id */
#define ASR_VENDOR_ID 0x424c

static const struct sdio_device_id asr_sdio_ids[] = {
#if defined(CONFIG_ASR5531) || defined(BASS_SUPPORT)
	{SDIO_DEVICE(ASR_VENDOR_ID, SD_DEVICE_ID_5531_FN1)},
#else
	{SDIO_DEVICE(0x424c, 0x6006)},
	{SDIO_DEVICE(0x02df, 0x912d)},
#endif
	{},
};

#ifdef CONFIG_PM

 /** mlan_status */
typedef enum _mlan_status {
	MLAN_STATUS_FAILURE = 0xffffffff,
	MLAN_STATUS_SUCCESS = 0,
	MLAN_STATUS_PENDING,
	MLAN_STATUS_RESOURCE,
} mlan_status;

/** mlan_ds_ps_info */
typedef struct _mlan_ds_ps_info {
    /** suspend allowed flag */
	u32 is_suspend_allowed;
} mlan_ds_ps_info;

/** moal_wait_option */
enum {
	MOAL_NO_WAIT,
	MOAL_IOCTL_WAIT,
	MOAL_CMD_WAIT,
#ifdef CONFIG_PROC_FS
	MOAL_PROC_WAIT,
#endif
	MOAL_WSTATS_WAIT,
	MOAL_IOCTL_WAIT_TIMEOUT
};

#define ENTER()     \
do {                \
    asr_dbg(SDIO,"Enter: %s\n", __func__);   \
} while (0)

#define LEAVE()     \
do {                \
    asr_dbg(SDIO,"Leave: %s\n", __func__);   \
} while (0)

/** MLAN TRUE */
#define MTRUE                    (1)
/** MLAN FALSE */
#define MFALSE                   (0)

/** PM keep power */
int pm_keep_power = 1;
/** HS when shutdown */
int shutdown_hs;

/**
 *  @brief Check driver status
 *
 *  @param handle   A pointer to moal_handle
 *
 *  @return         MTRUE/MFALSE
 */
u8 asr_check_driver_status(struct asr_hw *asr_hw)
{
#if 1
	ENTER();
#else
	struct asr_vif *priv = NULL;
	struct timeval t;
	int i = 0;

	priv = asr_get_vif(asr_hw, MLAN_BSS_ROLE_ANY);

	if (!priv || woal_get_debug_info(priv, MOAL_CMD_WAIT, &info)) {
		PRINTM(MERROR, "Could not retrieve debug information from MLAN\n");
		LEAVE();
		return MTRUE;
	}
#define MOAL_CMD_TIMEOUT_MAX            9
	do_gettimeofday(&t);
	if (info.pending_cmd && (t.tv_sec > (info.dnld_cmd_in_secs + MOAL_CMD_TIMEOUT_MAX))) {
		PRINTM(MERROR, "Timeout cmd id = 0x%x wait=%d\n",
		       info.pending_cmd, (int)(t.tv_sec - info.dnld_cmd_in_secs));
		LEAVE();
		return MTRUE;
	}
	if (info.num_cmd_timeout) {
		PRINTM(MERROR, "num_cmd_timeout = %d\n", info.num_cmd_timeout);
		PRINTM(MERROR, "Timeout cmd id = 0x%x, act = 0x%x\n", info.timeout_cmd_id, info.timeout_cmd_act);
		LEAVE();
		return MTRUE;
	}
	if (info.num_cmd_host_to_card_failure) {
		PRINTM(MERROR, "num_cmd_host_to_card_failure = %d\n", info.num_cmd_host_to_card_failure);
		LEAVE();
		return MTRUE;
	}
	if (info.num_no_cmd_node) {
		PRINTM(MERROR, "num_no_cmd_node = %d\n", info.num_no_cmd_node);
		LEAVE();
		return MTRUE;
	}
	for (i = 0; i < handle->priv_num; i++) {
		priv = handle->priv[i];
		if (priv) {
			if (priv->num_tx_timeout >= NUM_TX_TIMEOUT_THRESHOLD) {
				PRINTM(MERROR, "num_tx_timeout = %d\n", priv->num_tx_timeout);
				LEAVE();
				return MTRUE;
			}
		}
	}
	if (info.pm_wakeup_card_req && info.pm_wakeup_fw_try) {
#define MAX_WAIT_TIME     3
		if (t.tv_sec > (info.pm_wakeup_in_secs + MAX_WAIT_TIME)) {
			PRINTM(MERROR,
			       "wakeup_dev_req=%d wakeup_tries=%d wait=%d\n",
			       info.pm_wakeup_card_req, info.pm_wakeup_fw_try,
			       (int)(t.tv_sec - info.pm_wakeup_in_secs));
			LEAVE();
			return MTRUE;
		}
	}
#endif

	LEAVE();
	return MFALSE;

}

/** MLAN BSS role */
typedef enum _mlan_bss_role {
	MLAN_BSS_ROLE_STA = 0,
	MLAN_BSS_ROLE_UAP = 1,
	MLAN_BSS_ROLE_ANY = 0xff,
} mlan_bss_role;

/**
 *  @brief This function returns first available priv
 *  based on the BSS role
 *
 *  @param handle    A pointer to moal_handle
 *  @param bss_role  BSS role or MLAN_BSS_ROLE_ANY
 *
 *  @return          Pointer to moal_private
 */

#define GET_BSS_ROLE(priv) ((priv)->wdev.iftype)

struct asr_vif *asr_get_vif(struct asr_hw *asr_hw, mlan_bss_role bss_role)
{
	int i;
	enum nl80211_iftype iftype = NL80211_IFTYPE_MAX;

	switch (bss_role) {
	case MLAN_BSS_ROLE_STA:
		iftype = NL80211_IFTYPE_STATION;
		break;
	case MLAN_BSS_ROLE_UAP:
		iftype = NL80211_IFTYPE_AP;
		break;
	default:
		iftype = NL80211_IFTYPE_MAX;
	}

	for (i = 0; i < (asr_hw->vif_max_num + asr_hw->sta_max_num); i++) {
		if (asr_hw->vif_table[i]) {
			if ((bss_role == MLAN_BSS_ROLE_ANY) &&
			    ((GET_BSS_ROLE(asr_hw->vif_table[i]) == NL80211_IFTYPE_STATION)
			     || (GET_BSS_ROLE(asr_hw->vif_table[i]) == NL80211_IFTYPE_AP)))
				return asr_hw->vif_table[i];
			else if (GET_BSS_ROLE(asr_hw->vif_table[i]) == iftype)
				return asr_hw->vif_table[i];
		}
	}
	return NULL;
}

/**
 *  @brief Get PM info
 *
 *  @param priv                 A pointer to moal_private structure
 *  @param pm_info              A pointer to mlan_ds_ps_info structure
 *
 *  @return                     MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
mlan_status asr_get_pm_info(struct asr_vif * asr_vif, mlan_ds_ps_info * pm_info)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	//mlan_ds_pm_cfg *pmcfg = NULL;
	//mlan_ioctl_req *req = NULL;
	mlan_ds_ps_info ps_info = { 0 };

	ENTER();

#if 1
	ps_info.is_suspend_allowed = MTRUE;
	if (pm_info) {
		memcpy(pm_info, &ps_info, sizeof(mlan_ds_ps_info));
	}
#else
	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_pm_cfg));
	if (req == NULL) {
		PRINTM(MERROR, "Fail to alloc mlan_ds_pm_cfg buffer\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}

	/* Fill request buffer */
	pmcfg = (mlan_ds_pm_cfg *) req->pbuf;
	pmcfg->sub_command = MLAN_OID_PM_INFO;
	req->req_id = MLAN_IOCTL_PM_CFG;
	req->action = MLAN_ACT_GET;

	/* Send IOCTL request to MLAN */
	ret = woal_request_ioctl(priv, req, MOAL_CMD_WAIT);
	if (ret == MLAN_STATUS_SUCCESS) {
		if (pm_info) {
			memcpy(pm_info, &pmcfg->param.ps_info, sizeof(mlan_ds_ps_info));
		}
	}
done:
	if (ret != MLAN_STATUS_PENDING)
		kfree(req);

#endif

	LEAVE();
	return ret;
}

#ifdef MMC_PM_FUNC_SUSPENDED
/**  @brief This function tells lower driver that WLAN is suspended
 *
 *  @param handle   A Pointer to the moal_handle structure
 *  @return         N/A
 */
void asr_wlan_is_suspended(struct asr_hw *asr_hw)
{
	ENTER();
	if (asr_hw->suspend_notify_req == MTRUE) {
		asr_hw->is_suspended = MTRUE;
		sdio_func_suspended(((struct asr_plat *)asr_hw->plat)->func);
	}
	LEAVE();
}
#endif

/** Type definition of mlan_ds_hs_cfg for MLAN_OID_PM_CFG_HS_CFG */
typedef struct _mlan_ds_hs_cfg {
    /** MTRUE to invoke the HostCmd, MFALSE otherwise */
	u32 is_invoke_hostcmd;
    /** Host sleep config condition */
    /** Bit0: broadcast data
     *  Bit1: unicast data
     *  Bit2: mac event
     *  Bit3: multicast data
     */
	u32 conditions;
    /** GPIO pin or 0xff for interface */
	u32 gpio;
    /** Gap in milliseconds or or 0xff for special
     *  setting when GPIO is used to wakeup host
     */
	u32 gap;
} mlan_ds_hs_cfg, *pmlan_ds_hs_cfg;

/**
 *  @brief set bgscan config
 *
 *  @param handle               A pointer to moal_handle structure
 *
 *  @return                     MLAN_STATUS_SUCCESS -- success, otherwise fail
 */
void asr_reconfig_bgscan(struct asr_hw *asr_hw)
{
	int i;
	for (i = 0; i < (asr_hw->vif_max_num + asr_hw->sta_max_num); i++) {
		if (asr_hw->vif_table[i] && (GET_BSS_ROLE(asr_hw->vif_table[i]) == NL80211_IFTYPE_STATION)) {
#if 0
			if (asr_hw->vif_table[i]->bg_scan_start && asr_hw->vif_table[i]->bg_scan_reported) {
				asr_dbg(SDIO, "Reconfig BGSCAN\n");
				woal_request_bgscan(handle->priv[i], MOAL_NO_WAIT, &asr_hw->vif_table[i]->scan_cfg);
				asr_hw->vif_table[i]->bg_scan_reported = MFALSE;
			}
#endif
		}
	}
}

/**
 *  @brief Set auto arp resp
 *
 *  @param handle         A pointer to moal_handle structure
 *  @param enable         enable/disable
 *
 *  @return               MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */
static mlan_status asr_set_auto_arp(struct asr_hw *asr_hw, u8 enable)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;

#if 1
	ENTER();
#else

	int i = 0;
	struct asr_vif *priv = NULL;

	mlan_ds_misc_cfg *misc = NULL;
	mlan_ioctl_req *req = NULL;
	mlan_ds_misc_ipaddr_cfg ipaddr_cfg;

	memset(&ipaddr_cfg, 0, sizeof(ipaddr_cfg));

	for (i = 0; i < (NX_VIRT_DEV_MAX + NX_REMOTE_STA_MAX)
	     && (priv = asr_hw->vif_table[i]); i++) {
		if (priv->ip_addr_type != IPADDR_TYPE_NONE) {
			memcpy(ipaddr_cfg.ip_addr[ipaddr_cfg.ip_addr_num], priv->ip_addr, IPADDR_LEN);
			ipaddr_cfg.ip_addr_num++;
		}
	}
	if (ipaddr_cfg.ip_addr_num == 0) {
		asr_dbg(SDIO, "No IP addr configured.\n");
		goto done;
	}

	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_misc_cfg));
	if (req == NULL) {
		asr_dbg(SDIO, "IOCTL req allocated failed!\n");
		ret = MLAN_STATUS_FAILURE;
		goto done;
	}
	misc = (mlan_ds_misc_cfg *) req->pbuf;
	misc->sub_command = MLAN_OID_MISC_IP_ADDR;
	req->req_id = MLAN_IOCTL_MISC_CFG;
	req->action = MLAN_ACT_SET;
	memcpy(&misc->param.ipaddr_cfg, &ipaddr_cfg, sizeof(ipaddr_cfg));
	if (enable) {
		misc->param.ipaddr_cfg.op_code = MLAN_IPADDR_OP_ARP_FILTER | MLAN_IPADDR_OP_AUTO_ARP_RESP;
		misc->param.ipaddr_cfg.ip_addr_type = IPADDR_TYPE_IPV4;
	} else {
    /** remove ip */
		misc->param.ipaddr_cfg.op_code = MLAN_IPADDR_OP_IP_REMOVE;
	}
	ret = woal_request_ioctl(asr_get_vif(asr_hw, MLAN_BSS_ROLE_ANY), req, MOAL_NO_WAIT);
	if (ret != MLAN_STATUS_SUCCESS && ret != MLAN_STATUS_PENDING)
		asr_dbg(SDIO, "Set auto arp IOCTL failed!\n");
done:
	if (ret != MLAN_STATUS_PENDING)
		kfree(req);

#endif

	LEAVE();
	return ret;
}

/**
 *  @brief Get Host Sleep parameters
 *
 *  @param priv         A pointer to moal_private structure
 *  @param action       Action: set or get
 *  @param wait_option  Wait option (MOAL_WAIT or MOAL_NO_WAIT)
 *  @param hscfg        A pointer to mlan_ds_hs_cfg structure
 *
 *  @return             MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING -- success, otherwise fail
 */

/** Enumeration for the action of IOCTL request */
enum _mlan_act_ioctl {
	MLAN_ACT_SET = 1,
	MLAN_ACT_GET,
	MLAN_ACT_CANCEL
};

mlan_status asr_set_get_hs_params(struct asr_vif *priv, u16 action, u8 wait_option, mlan_ds_hs_cfg * hscfg)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	//mlan_ds_pm_cfg *pmcfg = NULL;
	//mlan_ioctl_req *req = NULL;

	ENTER();

#if 0
	/* Allocate an IOCTL request buffer */
	req = woal_alloc_mlan_ioctl_req(sizeof(mlan_ds_pm_cfg));
	if (req == NULL) {
		ret = -ENOMEM;
		goto done;
	}

	/* Fill request buffer */
	pmcfg = (mlan_ds_pm_cfg *) req->pbuf;
	pmcfg->sub_command = MLAN_OID_PM_CFG_HS_CFG;
	req->req_id = MLAN_IOCTL_PM_CFG;
	req->action = action;
	if (action == MLAN_ACT_SET)
		memcpy(&pmcfg->param.hs_cfg, hscfg, sizeof(mlan_ds_hs_cfg));

	/* Send IOCTL request to MLAN */
	ret = woal_request_ioctl(priv, req, wait_option);
	if (ret == MLAN_STATUS_SUCCESS) {
		if (hscfg && action == MLAN_ACT_GET) {
			memcpy(hscfg, &pmcfg->param.hs_cfg, sizeof(mlan_ds_hs_cfg));
		}
	}
done:
	if (ret != MLAN_STATUS_PENDING)
		kfree(req);
#endif

	LEAVE();
	return ret;
}

/** Host sleep config conditions : Cancel */
#define HOST_SLEEP_CFG_CANCEL   0xffffffff
/**
 *  @brief Cancel Host Sleep configuration
 *
 *  @param priv             A pointer to moal_private structure
 *  @param wait_option      wait option
 *
 *  @return                 MLAN_STATUS_SUCCESS, MLAN_STATUS_PENDING,
 *                              or MLAN_STATUS_FAILURE
 */
mlan_status asr_cancel_hs(struct asr_vif * priv, u8 wait_option)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_hs_cfg hscfg;
	struct asr_hw *asr_hw = priv->asr_hw;

	ENTER();

	/* Cancel Host Sleep */
	hscfg.conditions = HOST_SLEEP_CFG_CANCEL;
	hscfg.is_invoke_hostcmd = MTRUE;
	ret = asr_set_get_hs_params(priv, MLAN_ACT_SET, wait_option, &hscfg);

	/* remove auto arp from FW */
	asr_set_auto_arp(asr_hw, MFALSE);

	LEAVE();
	return ret;
}

/** hs active timeout 2 second */
#define HS_ACTIVE_TIMEOUT  (2 * HZ)

/**  @brief This function enables the host sleep
 *
 *  @param priv     A Pointer to the moal_private structure
 *  @return         MTRUE or MFALSE
 */
int asr_enable_hs(struct asr_vif *priv)
{
	int hs_actived = MFALSE;
	struct asr_hw *asr_hw;
	mlan_ds_hs_cfg hscfg;
	int timeout = 0;
	unsigned long to_now, to_end;
	mlan_ds_ps_info pm_info;

	ENTER();

	if (priv == NULL) {
		asr_dbg(SDIO, "Invalid priv\n");
		goto done;
	}

	asr_hw = priv->asr_hw;
	if (asr_hw == NULL) {
		asr_dbg(SDIO, "Invalid asr_hw\n");
		goto done;
	}

	if (asr_hw->hs_activated == MTRUE) {
		asr_dbg(SDIO, "HS Already actived\n");
		hs_actived = MTRUE;
		goto done;
	}
#if 0				//defined(WIFI_DIRECT_SUPPORT)
#if defined(STA_CFG80211) && defined(UAP_CFG80211)
#if LINUX_VERSION_CODE >= WIFI_DIRECT_KERNEL_VERSION
	if (asr_hw->is_remain_timer_set) {
		woal_cancel_timer(&priv->phandle->remain_timer);
		woal_remain_timer_func(priv->phandle);
	}
	/* cancel pending remain on channel */
	if (asr_hw->remain_on_channel) {
		u8 channel_status;
		moal_private *remain_priv = priv->phandle->priv[priv->phandle->remain_bss_index];
		if (remain_priv) {
			woal_cfg80211_remain_on_channel_cfg(remain_priv,
							    MOAL_NO_WAIT, MTRUE, &channel_status, NULL, 0, 0);
			if (priv->phandle->cookie) {
				cfg80211_remain_on_channel_expired(
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 6, 0)
									  remain_priv->netdev,
#else
									  remain_priv->wdev,
#endif
									  priv->phandle->cookie, &priv->phandle->chan,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
									  priv->phandle->channel_type,
#endif
									  GFP_ATOMIC);
				priv->phandle->cookie = 0;
			}
		}
		priv->phandle->remain_on_channel = MFALSE;
	}
#endif
#endif
#endif

#if 1				//def STA_SUPPORT
	asr_reconfig_bgscan(asr_hw);
#endif

	/* Set auto arp response configuration to Fw */
	asr_set_auto_arp(asr_hw, MTRUE);
	/* Enable Host Sleep */
	asr_hw->hs_activate_wait_q_woken = MFALSE;
	memset(&hscfg, 0, sizeof(mlan_ds_hs_cfg));
	hscfg.is_invoke_hostcmd = MTRUE;
	if (asr_set_get_hs_params(priv, MLAN_ACT_SET, MOAL_NO_WAIT, &hscfg) == MLAN_STATUS_FAILURE) {
		asr_dbg(SDIO, "IOCTL request HS enable failed\n");
		goto done;
	}

	to_now = jiffies;
	to_end = to_now + HS_ACTIVE_TIMEOUT;
again:
	timeout = wait_event_interruptible_timeout(asr_hw->hs_activate_wait_q,
						   asr_hw->hs_activate_wait_q_woken, to_end - to_now);
	if (timeout < 0) {
		to_now = jiffies;
		if (time_before(to_now, to_end))
			goto again;
		timeout = 0;
	}

	sdio_claim_host(((struct asr_plat *)asr_hw->plat)->func);
	if ((asr_hw->hs_activated == MTRUE)
	    || (asr_hw->is_suspended == MTRUE)) {
		asr_dbg(SDIO, "suspend success! force=%u skip=%u\n", asr_hw->hs_force_count, asr_hw->hs_skip_count);
		hs_actived = MTRUE;
	}
#ifdef CONFIG_PM
	else {
		asr_hw->suspend_fail = MTRUE;
		asr_get_pm_info(priv, &pm_info);
		if (pm_info.is_suspend_allowed == MTRUE) {
			asr_hw->hs_activated = MTRUE;
#ifdef MMC_PM_FUNC_SUSPENDED
			asr_wlan_is_suspended(asr_hw);
#endif
			asr_hw->hs_force_count++;
			asr_dbg(SDIO, "suspend allowed! force=%u skip=%u\n",
				asr_hw->hs_force_count, asr_hw->hs_skip_count);
			hs_actived = MTRUE;
		}
	}
#endif /* SDIO_SUSPEND_RESUME */
	sdio_release_host(((struct asr_plat *)asr_hw->plat)->func);
	if (hs_actived != MTRUE) {
		asr_hw->hs_skip_count++;
#ifdef CONFIG_PM
		asr_dbg(SDIO,
			"suspend skipped! timeout=%d allow=%d force=%u skip=%u\n",
			timeout, (int)pm_info.is_suspend_allowed, asr_hw->hs_force_count, asr_hw->hs_skip_count);
#else
		asr_dbg(SDIO, "suspend skipped! timeout=%d skip=%u\n", timeout, asr_hw->hs_skip_count);
#endif
		asr_cancel_hs(priv, MOAL_IOCTL_WAIT_TIMEOUT);
	}
done:

	LEAVE();
	return hs_actived;
}

/**  @brief This function handles client driver suspend
 *
 *  @param dev      A pointer to device structure
 *  @return         MLAN_STATUS_SUCCESS or error code
 */
int asr_sdio_suspend(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	mmc_pm_flag_t pm_flags = 0;
	//moal_handle *handle = NULL;
	//struct sdio_mmc_card *cardp;
	struct asr_hw *asr_hw;

	int i;
	int ret = MLAN_STATUS_SUCCESS;
	int hs_actived = 0;
	mlan_ds_ps_info pm_info;

	ENTER();
	asr_dbg(SDIO, "<--- Enter asr_sdio_suspend --->\n");
	pm_flags = sdio_get_host_pm_caps(func);
	asr_dbg(SDIO, "%s: suspend: PM flags = 0x%x\n", sdio_func_id(func), pm_flags);
	if (!(pm_flags & MMC_PM_KEEP_POWER)) {
		asr_dbg(SDIO, "%s: cannot remain alive while host is suspended\n", sdio_func_id(func));
		LEAVE();
		return -ENOSYS;
	}
	//cardp = sdio_get_drvdata(func);
	asr_hw = sdio_get_drvdata(func);

	if (!asr_hw) {
		asr_dbg(SDIO, "asr_hw structure is not valid\n");
		LEAVE();
		return MLAN_STATUS_SUCCESS;
	}

	if (asr_hw->is_suspended == MTRUE) {
		asr_dbg(SDIO, "Device already suspended\n");
		LEAVE();
		return MLAN_STATUS_SUCCESS;
	}

	if (asr_check_driver_status(asr_hw)) {
		asr_dbg(SDIO, "Allow suspend when device is in hang state\n");
#ifdef MMC_PM_SKIP_RESUME_PROBE
		asr_dbg(SDIO, "suspend with MMC_PM_KEEP_POWER and MMC_PM_SKIP_RESUME_PROBE\n");
		ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER | MMC_PM_SKIP_RESUME_PROBE);
#else
		asr_dbg(SDIO, "suspend with MMC_PM_KEEP_POWER\n");
		ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
#endif
		asr_hw->hs_force_count++;
		asr_hw->is_suspended = MTRUE;
		LEAVE();
		return MLAN_STATUS_SUCCESS;
	}

	asr_hw->suspend_fail = MFALSE;
	memset(&pm_info, 0, sizeof(pm_info));

#if 0
	if (MLAN_STATUS_SUCCESS == asr_get_pm_info(asr_get_vif(asr_hw, MLAN_BSS_ROLE_ANY), &pm_info)) {
		if (pm_info.is_suspend_allowed == MFALSE) {
			asr_dbg(SDIO, "suspend not allowed!\n");
			ret = -EBUSY;
			goto done;
		}
	}
#else
	pm_info.is_suspend_allowed = MTRUE;
#endif

	asr_dbg(SDIO, "suspend allowed!\n");

	for (i = 0; i < (asr_hw->vif_max_num + asr_hw->sta_max_num); i++) {
		if (asr_hw->vif_table[i] && (asr_hw->vif_table[i]->ndev)) {
			asr_dbg(SDIO, "netif_device_detach! i=%d\n", i);
			netif_device_detach(asr_hw->vif_table[i]->ndev);
		}
	}

	asr_dbg(SDIO, "start to enable the Host Sleep!");
	if (pm_keep_power) {
		/* Enable the Host Sleep */
#ifdef MMC_PM_FUNC_SUSPENDED
		asr_hw->suspend_notify_req = MTRUE;
#endif
		hs_actived = asr_enable_hs(asr_get_vif(asr_hw, MLAN_BSS_ROLE_ANY));
#ifdef MMC_PM_FUNC_SUSPENDED
		asr_hw->suspend_notify_req = MFALSE;
#endif
		if (hs_actived) {
#ifdef MMC_PM_SKIP_RESUME_PROBE
			asr_dbg(SDIO, "suspend with MMC_PM_KEEP_POWER and MMC_PM_SKIP_RESUME_PROBE\n");
			ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER | MMC_PM_SKIP_RESUME_PROBE);
#else
			asr_dbg(SDIO, "suspend with MMC_PM_KEEP_POWER\n");
			ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
#endif
		} else {
			asr_dbg(SDIO, "HS not actived, suspend fail!");
			asr_hw->suspend_fail = MTRUE;
			for (i = 0; i < (asr_hw->vif_max_num + asr_hw->sta_max_num); i++) {
				if (asr_hw->vif_table[i]
				    && (asr_hw->vif_table[i]->ndev))
					netif_device_attach(asr_hw->vif_table[i]->ndev);
			}
			ret = -EBUSY;
			goto done;
		}
	}

	/* Indicate device suspended */
	asr_hw->is_suspended = MTRUE;
done:
	asr_dbg(SDIO, "<--- Leave asr_sdio_suspend --->\n");
	LEAVE();
	return ret;
}

/**  @brief This function handles client driver resume
 *
 *  @param dev      A pointer to device structure
 *  @return         MLAN_STATUS_SUCCESS
 */
int asr_sdio_resume(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	mmc_pm_flag_t pm_flags = 0;
	//moal_handle *handle = NULL;
	//struct sdio_mmc_card *cardp;
	struct asr_hw *asr_hw;

	int i;

	ENTER();
	asr_dbg(SDIO, "<--- Enter asr_sdio_resume --->\n");
	pm_flags = sdio_get_host_pm_caps(func);
	asr_dbg(SDIO, "%s: resume: PM flags = 0x%x\n", sdio_func_id(func), pm_flags);
	//cardp = sdio_get_drvdata(func);
	asr_hw = sdio_get_drvdata(func);

	if (!asr_hw) {
		asr_dbg(SDIO, "Card or moal_handle structure is not valid\n");
		LEAVE();
		return MLAN_STATUS_SUCCESS;
	}

	if (asr_hw->is_suspended == MFALSE) {
		asr_dbg(SDIO, "Device already resumed\n");
		LEAVE();
		return MLAN_STATUS_SUCCESS;
	}
	asr_hw->is_suspended = MFALSE;
	if (asr_check_driver_status(asr_hw)) {
		asr_dbg(SDIO, "Resuem, device is in hang state\n");
		LEAVE();
		return MLAN_STATUS_SUCCESS;
	}
	for (i = 0; i < (asr_hw->vif_max_num + asr_hw->sta_max_num); i++)
		netif_device_attach(asr_hw->vif_table[i]->ndev);

	/* Disable Host Sleep */
	asr_cancel_hs(asr_get_vif(asr_hw, MLAN_BSS_ROLE_ANY), MOAL_NO_WAIT);
	asr_dbg(SDIO, "<--- Leave asr_sdio_resume --->\n");
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

const struct dev_pm_ops asr_sdio_pm_ops = {
	.suspend = asr_sdio_suspend,
	.resume = asr_sdio_resume,
};
#endif

static struct sdio_driver asr_sdio_drv = {
	.name = "asrsdio",
	.drv = {
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &asr_sdio_pm_ops,
#endif
		},
	.id_table = asr_sdio_ids,
	.probe = asr_sdio_probe,
	.remove = asr_sdio_remove,
};

int asr_sdio_register_drv(void)
{
	int ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	g_sdio_dev_detect_timer.function = sdio_dev_detect_timer_callback;

	mod_timer(&g_sdio_dev_detect_timer, jiffies + msecs_to_jiffies(800));	//wait for detect sdio dev
#else
	setup_timer(&g_sdio_dev_detect_timer, sdio_dev_detect_timer_callback, (unsigned long)"hello");
	g_sdio_dev_detect_timer.expires = jiffies + msecs_to_jiffies(800);
	add_timer(&g_sdio_dev_detect_timer);
#endif

	ret = sdio_register_driver(&asr_sdio_drv);
	if (ret)
		del_timer(&g_sdio_dev_detect_timer);
	return ret;
}

void asr_sdio_unregister_drv(void)
{
	del_timer(&g_sdio_dev_detect_timer);

	sdio_unregister_driver(&asr_sdio_drv);
}

u16 asr_get_tx_bitmap_from_ports(u8 start_port, u8 end_port)
{
	int i;
	u16 bitmap = 0;

	if (start_port <= end_port) {
		for (i = start_port; i <= end_port; i++) {
			bitmap |= (1 << i);
		}
	} else {
		for (i = start_port; i <= 15; i++) {
			bitmap |= (1 << i);
		}
		for (i = 1; i <= end_port; i++) {
			bitmap |= (1 << i);
		}
	}
	return bitmap;
}

u8 asr_get_tx_aggr_bitmap_addr(u8 start_port, u8 end_port)
{
	int i;
	u8 bitmap = 0;
	if (start_port <= end_port) {
		for (i = 0; i < end_port - start_port + 1; i++) {
			bitmap |= (1 << i);
		}
	} else {
		for (i = 0; i < end_port + 17 - start_port; i++) {
			if (start_port + i != 16)
				bitmap |= (1 << i);
		}
	}
	return bitmap;
}

extern volatile uint8_t dbg_err_ind;
int last_tx_time = 0;
extern int tx_conserve;
int asr_sdio_tx_common_port_dispatch(struct asr_hw *asr_hw, u8 * src, u32 len, unsigned int io_addr, u16 bitmap_record)
{
	int ret = 0;
	struct sdio_func *func = asr_hw->plat->func;

	sdio_claim_host(func);
	ret = sdio_writesb(func, io_addr, src, len);
	spin_lock_bh(&asr_hw->tx_msg_lock);
	asr_hw->tx_use_bitmap &= ~(bitmap_record);
	spin_unlock_bh(&asr_hw->tx_msg_lock);
#ifdef TXBM_DELAY_UPDATE
	asr_hw->tx_last_trans_bitmap = bitmap_record;
#endif
	sdio_release_host(func);

	//seconds = ktime_to_us(ktime_get());
	//dbg("tda %d\n%s",((int)ktime_to_us(ktime_get())-seconds), "\0");

	if (ret) {
		dev_err(g_asr_para.dev, "%s write data failed,ret=%d\n", __func__, ret);
	}

	return ret;
}

extern int tx_aggr;
#define MAX_AGG_THRESHOLD 1
u8 asr_sdio_tx_get_available_data_port(struct asr_hw *asr_hw,
				       u16 ava_pkt_num, u8 * port_num, unsigned int *io_addr, u16 * bitmap_record)
{
	u16 bitmap;
	u8 start_port;
	u8 end_port;
	uint8_t port_idx;
	//u16 bitmap_record = 0x0;
	*bitmap_record = 0;
#if 1
	if (ava_pkt_num > tx_aggr)
		ava_pkt_num = tx_aggr;
	*io_addr = 0x0;
	port_idx = 0x0;
	bitmap = asr_hw->tx_use_bitmap;

	if (bitmap & (1 << asr_hw->tx_data_cur_idx)) {
		start_port = asr_hw->tx_data_cur_idx;
		end_port = asr_hw->tx_data_cur_idx;
		*io_addr |= (1 << port_idx++);
		*port_num = 1;
		ava_pkt_num--;
		*bitmap_record |= (1 << asr_hw->tx_data_cur_idx);

		if ((++asr_hw->tx_data_cur_idx) == 16) {
			asr_hw->tx_data_cur_idx = 1;
			if (ava_pkt_num) {
				//ava_pkt_num--;
				*io_addr &= ~(1 << port_idx++);
			}
		}
	} else {
		return 0;
	}

	while (ava_pkt_num--) {
		if (bitmap & (1 << asr_hw->tx_data_cur_idx)) {
			*bitmap_record |= (1 << asr_hw->tx_data_cur_idx);
			end_port = asr_hw->tx_data_cur_idx;
			(*port_num)++;
			*io_addr |= (1 << port_idx++);
			if ((++asr_hw->tx_data_cur_idx) == 16) {
				asr_hw->tx_data_cur_idx = 1;
				if (ava_pkt_num) {
					//ava_pkt_num--;
					*io_addr &= ~(1 << port_idx++);
				}
			}
		} else
			break;
		if (port_idx >= tx_aggr)
			break;
	}
	if (*port_num == 1)
		*io_addr = asr_hw->ioport | start_port;
	else
		*io_addr = asr_hw->ioport | 0x1000 | start_port | ((*io_addr) << 4);

	return *port_num;
#else
	bitmap = asr_hw->tx_use_bitmap;

	if (bitmap & (1 << asr_hw->tx_data_cur_idx)) {
		*start_port = asr_hw->tx_data_cur_idx;
		*end_port = asr_hw->tx_data_cur_idx;

		if (++asr_hw->tx_data_cur_idx == 16)
			asr_hw->tx_data_cur_idx = 1;
		if (ava_pkt_num > tx_aggr)
			*port_num = tx_aggr;
		else
			*port_num = ava_pkt_num;
	} else {
		*port_num = 0;
	}
	return *port_num;
#endif
}

bool asr_sdio_tx_msg_port_available(struct asr_hw * asr_hw)
{
	bool ret;
	spin_lock_bh(&asr_hw->tx_msg_lock);
	ret = (asr_hw->tx_use_bitmap & (1 << SDIO_PORT_IDX(0))) ? true : false;
	asr_hw->tx_use_bitmap &= ~(1 << SDIO_PORT_IDX(0));
	spin_unlock_bh(&asr_hw->tx_msg_lock);
	return ret;
}

int asr_sdio_send_data(struct asr_hw *asr_hw, u8 type, u8 * src, u16 len, unsigned int io_addr, u16 bitmap_record)
{
	int ret = 0;
	static u32 tx_msg_jiffies = 0;
	u64 txmsg_ms = 0;
	struct lmac_msg *tx_msg = (struct lmac_msg *)src;

	ASR_DBG(ASR_FN_ENTRY_STR);

	if (type == HIF_TX_MSG)	//msg
	{
		int count = 20;

		if (g_asr_para.sdio_send_times == 0)	//first time,poll state
		{
			g_asr_para.sdio_send_times++;
			ret = poll_card_status(asr_hw->plat->func, C2H_IO_RDY);
			if (ret) {
				dev_err(asr_hw->dev, "%s card is not in rdy\n", __func__);
				return -EBUSY;
			}
		} else {
			while ((!(asr_sdio_tx_msg_port_available(asr_hw))) && (--count))	//no available msg port 0
			{
				//dev_err(g_asr_para.dev, "no tx msg port\n");
				msleep(1);
			}
			if (count == 0) {
				dev_err(asr_hw->dev,
					"ERROR: msg port 0 busy! msg(%d,%d)\n", MSG_T(tx_msg->id), MSG_I(tx_msg->id));
				return -EBUSY;
			}
		}

		//adjust len, must be block size blocks
		len = len + 4;	//  4 is for end token
		//if (len > SDIO_BLOCK_SIZE)
		len = ASR_ALIGN_BLKSZ_HI(len);

		mutex_lock(&asr_hw->tx_msg_mutex);
		if (jiffies >= tx_msg_jiffies) {
			txmsg_ms = jiffies_to_msecs(jiffies - tx_msg_jiffies);
		} else {
			txmsg_ms = jiffies_to_msecs(jiffies);
		}

		while (txmsg_ms < 20) {	//delay 20 ms
			//dev_info(asr_hw->dev,"%s: msg delay %lu,%lu,%llu\n",__func__
			//      , (long unsigned int)tx_msg_jiffies, (long unsigned int)jiffies, (long long unsigned int)txmsg_ms);
			msleep(1);	//fix no tx msg port bug

			if (jiffies >= tx_msg_jiffies) {
				txmsg_ms = jiffies_to_msecs(jiffies - tx_msg_jiffies);
			} else {
				txmsg_ms = jiffies_to_msecs(jiffies);
			}
		}
		tx_msg_jiffies = jiffies;
		mutex_unlock(&asr_hw->tx_msg_mutex);

#if 0
		uint32_t *temp = (uint32_t *) src;
		dev_info(asr_hw->dev,
			 "tx msg %u(len=%d)%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
			 tx_msg_jiffies, len, temp[0], temp[1], temp[2],
			 temp[3], temp[4], temp[5], temp[6], temp[7], temp[8],
			 temp[9], temp[10], temp[11], temp[12], temp[13], temp[14], temp[15]);
#else

		dev_info(asr_hw->dev, "%s: tx msg %d,%d\n", __func__, MSG_T(tx_msg->id), MSG_I(tx_msg->id));
#endif

		ret = asr_sdio_tx_common_port_dispatch(asr_hw, src, len, io_addr, 0x1);
	} else			//data
	{
		//if (len % SDIO_BLOCK_SIZE)
		//    dev_err(g_asr_para.dev, "tx len error %d\n",len)
		ret = asr_sdio_tx_common_port_dispatch(asr_hw, src, len, io_addr, bitmap_record);
#ifdef CONFIG_ASR_KEY_DBG
		if (txlogen)
			dev_info(g_asr_para.dev, "uti %x\n", asr_hw->tx_data_cur_idx);
#endif
	}

	return ret;
}
