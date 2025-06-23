/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Define the register and struct for ArtInChip DVP controller.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#ifndef _AIC_DVP_H_
#define _AIC_DVP_H_

#include <media/media-device.h>
#include <media/v4l2-async.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>

#define AIC_DVP_NAME	"aic-dvp"

#define DVP_MAX_BUF		3
#define DVP_MAX_PLANE		2
#define DVP_MAX_HEIGHT		4096U
#define DVP_MAX_WIDTH		4096U

enum dvp_input {
	DVP_IN_RAW	= 0,
	DVP_IN_YUV422	= 1,
	DVP_IN_BT656	= 2,
};

enum dvp_output {
	DVP_OUT_RAW_PASSTHROUGH		= 0,
	DVP_OUT_YUV422_COMBINED_NV16	= 1,
	DVP_OUT_YUV420_COMBINED_NV12	= 2,
};

enum dvp_input_yuv_seq {
	DVP_YUV_DATA_SEQ_YUYV	= 0,
	DVP_YUV_DATA_SEQ_YVYU	= 1,
	DVP_YUV_DATA_SEQ_UYVY	= 2,
	DVP_YUV_DATA_SEQ_VYUY	= 3,
};

enum dvp_capture_mode {
	DVP_CAPTURE_PICTURE = 0,
	DVP_CAPTURE_VIDEO = 1
};

enum dvp_subdev_pads {
	DVP_SUBDEV_SINK = 0,
	DVP_SUBDEV_SOURCE,
	DVP_SUBDEV_PAD_NUM,
};

#define DVP_CH_BASE(ch)			(0x100 * ((ch) + 1))
#define DVP_CTL				0x0
#define DVP_IRQ_EN(ch)			(DVP_CH_BASE(ch) + 0x00)
#define DVP_IRQ_STA(ch)			(DVP_CH_BASE(ch) + 0x04)
#define DVP_IRQ_CFG(ch)			(DVP_CH_BASE(ch) + 0x08)
#define DVP_IN_CFG(ch)			(DVP_CH_BASE(ch) + 0x0C)
#define DVP_IN_HOR_SIZE(ch)		(DVP_CH_BASE(ch) + 0x10)
#define DVP_IN_VER_SIZE(ch)		(DVP_CH_BASE(ch) + 0x14)
#define DVP_OUT_HOR_SIZE(ch)		(DVP_CH_BASE(ch) + 0x20)
#define DVP_OUT_FIFO_LEVEL(ch)		(DVP_CH_BASE(ch) + 0x24)
#define DVP_OUT_VER_SIZE(ch)		(DVP_CH_BASE(ch) + 0x28)
#define DVP_OUT_QOS_CFG(ch)		(DVP_CH_BASE(ch) + 0x2C)
#define DVP_OUT_FRA_NUM(ch)		(DVP_CH_BASE(ch) + 0x30)
#define DVP_OUT_CUR_FRA(ch)		(DVP_CH_BASE(ch) + 0x34)
#define DVP_OUT_CTL(ch)			(DVP_CH_BASE(ch) + 0x38)
#define DVP_OUT_UPDATE_CTL(ch)		(DVP_CH_BASE(ch) + 0x3C)
#define DVP_OUT_ADDR_BUF0(ch)		(DVP_CH_BASE(ch) + 0x40)
#define DVP_OUT_ADDR_BUF1(ch)		(DVP_CH_BASE(ch) + 0x44)
#define DVP_OUT_READ_ADDR0(ch)		(DVP_CH_BASE(ch) + 0x48)
#define DVP_OUT_READ_ADDR1(ch)		(DVP_CH_BASE(ch) + 0x4C)
#define DVP_OUT_LINE_STRIDE0(ch)	(DVP_CH_BASE(ch) + 0x50)
#define DVP_OUT_LINE_STRIDE1(ch)	(DVP_CH_BASE(ch) + 0x54)
#define DVP_OUT_ADDR_BUF0_SHA(ch)	(DVP_CH_BASE(ch) + 0x58)
#define DVP_OUT_ADDR_BUF1_SHA(ch)	(DVP_CH_BASE(ch) + 0x5C)
#define DVP_OUT_LINE_STRIDE_SHA(ch)	(DVP_CH_BASE(ch) + 0x60)
#define DVP_VER				0xFFC

#define DVP_CTL_OUT_FMT(v)		((v) << 12)
#define DVP_CTL_OUT_FMT_MASK		GENMASK(14, 12)
#define DVP_CTL_IN_SEQ(v)		((v) << 8)
#define DVP_CTL_IN_SEQ_MASK		GENMASK(9, 8)
#define DVP_CTL_IN_FMT(v)		((v) << 4)
#define DVP_CTL_IN_FMT_MASK		GENMASK(6, 4)
#define DVP_CTL_DROP_FRAME_EN		BIT(2)
#define DVP_CTL_CLR			BIT(1)
#define DVP_CTL_EN			BIT(0)

#define DVP_IRQ_EN_UPDATE_DONE		BIT(7)
#define DVP_IRQ_EN_HNUM			BIT(2)
#define DVP_IRQ_EN_FRAME_DONE		BIT(1)

#define DVP_IRQ_STA_UPDATE_DONE		BIT(7)
#define DVP_IRQ_STA_XY_CODE_ERR		BIT(6)
#define DVP_IRQ_STA_BUF_FULL		BIT(3)
#define DVP_IRQ_STA_HNUM		BIT(2)
#define DVP_IRQ_STA_FRAME_DONE		BIT(1)

#define DVP_IRQ_CFG_HNUM_MASK		GENMASK(30, 16)
#define DVP_IRQ_CFG_HNUM_SHIFT		16

#define DVP_IN_CFG_FILED_POL_ACTIVE_LOW		BIT(3)
#define DVP_IN_CFG_VSYNC_POL_FALLING		BIT(2)
#define DVP_IN_CFG_HREF_POL_ACTIVE_HIGH		BIT(1)
#define DVP_IN_CFG_PCLK_POL_FALLING		BIT(0)

/* The field definition of IN_HOR_SIZE */
#define DVP_IN_HOR_SIZE_IN_HOR_MASK		GENMASK(30, 16)
#define DVP_IN_HOR_SIZE_IN_HOR_SHIFT		(16)
#define DVP_IN_HOR_SIZE_XY_CODE_ERR_MASK	GENMASK(15, 8)
#define DVP_IN_HOR_SIZE_XY_CODE_ERR_SHIFT	(8)
#define DVP_IN_HOR_SIZE_XY_CODE_MASK		GENMASK(7, 0)
#define DVP_IN_HOR_SIZE_XY_CODE_SHIFT		(0)
#define DVP_IN_HOR_SIZE_XY_CODE_F		BIT(6)

#define DVP_OUT_HOR_NUM(w, s)		(((w + s) * 2 - 1) << 16 | s)
#define DVP_OUT_VER_NUM(h, s)		(((h + s) - 1) << 16 | s)

#define DVP_OUT_ADDR_BUF(ch, plane)	(plane ? DVP_OUT_ADDR_BUF1(ch) \
						: DVP_OUT_ADDR_BUF0(ch))

#define DVP_OUT_CTL_CAP_OFF_IMMEDIATELY	BIT(1)
#define DVP_OUT_CTL_CAP_ON		BIT(0)

extern const struct v4l2_subdev_ops aic_dvp_subdev_ops;

/**
 * Save the configuration information for DVP controller.
 * @code:	media bus format code (MEDIA_BUS_FMT_*, in media-bus-format.h)
 * @field:	used interlacing type (enum v4l2_field)
 * @width:	frame width
 * @height:	frame height
 */
struct aic_dvp_config {
	/* Input format */
	enum dvp_input		input;
	enum dvp_input_yuv_seq	input_seq;
	enum v4l2_field		field;
	int			field_active;

	/* Output format */
	enum dvp_output	output;
	u32		width;
	u32		height;
	u32		stride[DVP_MAX_PLANE];
	u32		sizeimage[DVP_MAX_PLANE];
};

struct aic_dvp {
	/* Device resources */
	struct device			*dev;

	void __iomem			*regs;
	struct clk			*clk;
	struct reset_control		*rst;
	u32				clk_rate;
	int				irq;
	int				ch;

	struct vb2_v4l2_buffer		*vbuf[DVP_MAX_BUF];

	struct aic_dvp_config		cfg; /* The configuration of DVP HW */
	struct v4l2_fwnode_bus_parallel	bus; /* The format of input data */
	struct v4l2_pix_format_mplane	fmt; /* The format of output data */

	/* Main Device */
	struct v4l2_device		v4l2;
	struct media_device		mdev;
	struct video_device		vdev;
	struct media_pad		vdev_pad;

	/* Local subdev */
	struct v4l2_subdev		subdev;
	struct media_pad		subdev_pads[DVP_SUBDEV_PAD_NUM];
	struct v4l2_mbus_framefmt	subdev_fmt;

	/* V4L2 Async variables */
	struct v4l2_async_subdev	asd;
	struct v4l2_async_notifier	notifier;
	struct v4l2_subdev		*src_subdev;
	int				src_pad;

	/* V4L2 variables */
	struct mutex			lock;

	/* Videobuf2 */
	struct vb2_queue		queue;
	struct list_head		buf_list;
	spinlock_t			qlock;
	unsigned int			sequence;
	unsigned int			streaming;
};

struct aic_dvp_buf {
	struct vb2_v4l2_buffer	vb;
	struct list_head	list;
	dma_addr_t		paddr[DVP_MAX_PLANE];
	bool			dvp_using;
};

int aic_dvp_buf_register(struct aic_dvp *dvp);
void aic_dvp_buf_unregister(struct aic_dvp *dvp);
int aic_dvp_video_register(struct aic_dvp *dvp);

/* Some API of register, Defined in aic_dvp_hw.c */
void aic_dvp_enable(struct aic_dvp *dvp, int enable);
void aic_dvp_capture_start(struct aic_dvp *dvp);
void aic_dvp_capture_stop(struct aic_dvp *dvp);
void aic_dvp_clr_fifo(struct aic_dvp *dvp);
int aic_dvp_clr_int(struct aic_dvp *dvp);
void aic_dvp_enable_int(struct aic_dvp *dvp, int enable);
void aic_dvp_set_pol(struct aic_dvp *dvp);
void aic_dvp_set_cfg(struct aic_dvp *dvp);
void aic_dvp_update_buf_addr(struct aic_dvp *dvp, struct aic_dvp_buf *buf,
			     u32 offset);
void aic_dvp_update_ctl(struct aic_dvp *dvp);
u32 aic_dvp_get_current_xy(struct aic_dvp *dvp);
u32 aic_dvp_is_top_field(void);
u32 aic_dvp_is_bottom_field(void);
void aic_dvp_field_tag_clr(void);

#endif /* _AIC_DVP_H_ */
