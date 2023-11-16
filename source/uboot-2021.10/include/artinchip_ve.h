/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 ArtInChip Technology Co.,Ltd
 * Author: Hao Xiong <hao.xiong@artinchip.com>
 */

#ifndef _ARTINCHIP_VE_H_
#define _ARTINCHIP_VE_H_

#define VE_CLK_REG		0x00
#define VE_RST_REG		0x04
#define VE_INIT_REG		0x08
#define VE_IRQ_REG		0x0C
#define VE_PNG_EN_REG		0x18

#define INFLATE_INT_REG		0xC00
#define INFLATE_STATUS_REG	0xC04
#define INFLATE_START_REG	0xC08
#define INFLATE_CTRL_REG	0xC10
#define INFLATE_CHECK_REG	0xC3C
#define INFLATE_WINDOW_BUF_REG	0xC40
#define INFLATE_RESET_REG	0xC50
#define INFLATE_CYCLE_REG	0xCC0

#define INPUT_BS_START_ADDR_REG	0xC20
#define INPUT_BS_END_ADDR_REG	0xC24
#define INPUT_BS_OFFSET_REG	0xC28
#define INPUT_BS_LENGTH_REG	0xC2C
#define INPUT_BS_DATA_VALID_REG	0xC48

#define OUTPUT_BUF_ADDR_REG	0xC30
#define OUTPUT_MAX_LENGTH_REG	0xC34
#define OUTPUT_COUNT_REG	0xC38

#define VE_CLK_BIT_EN	BIT(0)
#define	VE_INIT_BIT_EN	BIT(0)
#define VE_IRQ_BIT_EN	BIT(0)
#define	VE_PNG_BIT_EN	BIT(0)
#define VE_RST_MSK	GENMASK(5, 4)

#define INFLATE_INT_BIT_DEC_FINISH	BIT(0)
#define INFLATE_INT_BIT_DEC_ERR		BIT(1)
#define INFLATE_INT_BIT_DEC_BIT_REQ	BIT(2)
#define INFLATE_INT_BIT_DEC_OVER_TIME	BIT(3)

#define INFLATE_STATUS_BIT_FINISH	BIT(0)
#define INFLATE_STATUS_BIT_ERR		BIT(1)
#define INFLATE_STATUS_BIT_REQ		BIT(2)
#define INFLATE_STATUS_BIT_OVER_TIME	BIT(3)
#define INFLATE_STATUS_BIT_OUT_BUF_OVER	BIT(4)
#define INFLATE_STATUS_BIT_PC_ERR	BIT(5)
#define INFLATE_STATUS_BIT_GZ_ERR	BIT(6)
#define INFLATE_STATUS_BIT_DH_ERR	BIT(7)
#define INFLATE_STATUS_BIT_CL_ERR	BIT(8)
#define INFLATE_STATUS_BIT_LZ77_ERR_MSK	BIT(13, 9)
#define INFLATE_STATUS_BIT_RDMA_ERR	BIT(14)
#define INFLATE_STATUS_BIT_DCTRL_ERR	BIT(15)

#define INFLATE_START_BIT_EN	BIT(0)
#define INFLATE_RESET_BIT_EN	BIT(0)

#define INFLATE_CTRL_BIT_DEC_TYPE_MSK	GENMASK(1, 0)
#define INFLATE_CTRL_BIT_CHECK_FUNC_MSK	GENMASK(3, 2)
#define INFLATE_CTRL_GZIP_DECODE_TYPE	0

#define INFLATE_CHECK_BIT_CHECK_SUM_MSK	GENMASK(31, 0)

#define INFLATE_WINDOW_BUF_BIT_ADDR_MSK	GENMASK(31, 10)

#define INPUT_BS_START_ADDR_MSK		GENMASK(31, 4)
#define INPUT_BS_START_ADDR_MSK_OFF	4
#define INPUT_BS_END_ADDR_MSK		GENMASK(31, 4)
#define INPUT_BS_END_ADDR_MSK_OFF	4
#define INPUT_BS_OFFSET_MSK		GENMASK(29, 0)

#define OUTPUT_BUF_ADDR_MSK		GENMASK(31, 0)
#define OUTPUT_MAX_LENGTH_MSK		GENMASK(31, 0)
#define OUTPUT_COUNT_MSK		GENMASK(31, 0)

#define DECODE_ALIGN			1024

int gunzip_init_device(struct udevice **dev);

int gunzip_decompress(struct udevice *dev, void *dst, int dstlen,
			unsigned char *src, unsigned long *lenp);

int __png_decode(struct udevice *dev, void *src, unsigned int size);
int __jpeg_decode(struct udevice *dev, void *src, unsigned int size);

int aic_png_decode(void *src, unsigned int size);
int aic_jpeg_decode(void *src, unsigned int size);

/*
 * struct decoder_ops - Driver model ArtInChip decoder operations
 *
 * The uclass interface is implemented by all decoder devices which
 * use driver model.
 */
struct decoder_ops {
	/**
	 * Init decoder device and enable decoder clk
	 * @dev: the device
	 *
	 * Return: 0 if OK, -ve on error
	 */
	int (*init)(struct udevice *dev);
	/**
	 * Preparing gzip parameters.
	 * @dev: the device
	 * @dst: Destination for uncompressed data
	 * @dstlen: Size of destination buffer
	 * @src: Source data to decompress
	 * @lenp: Returns length of uncompressed data
	 *
	 * Return: 0 if OK, -1 on error
	 */
	int (*decompress_init)(struct udevice *dev, void *dst, int dstlen,
				unsigned char *src, unsigned long *lenp);
	/**
	 * Decompress gzipped data.
	 * @dev: the device
	 *
	 * Return: 0 if OK, -1 on error
	 */
	int (*decompress)(struct udevice *dev);

	/**
	 * Decode PNG image
	 * @dev: the device
	 * @src:  Start of the source buffer.
	 * @size: Size of the source buffer.
	 *
	 * @return 0 if OK, -ve on error
	 */
	int (*png_decode)(struct udevice *dev,
				void *src, unsigned int size);
	/**
	 * Decode JPEG image
	 * @dev: the device
	 * @src:  Start of the source buffer.
	 * @size: Size of the source buffer.
	 *
	 * @return 0 if OK, -ve on error
	 */
	int (*jpeg_decode)(struct udevice *dev,
				void *src, unsigned int size);
	/**
	 * Release decoder device
	 * @dev: the device
	 *
	 */
	void (*release)(struct udevice *dev);
};

#endif	/* _ARTINCHIP_VE_H_  */
