// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/miscdevice.h>
#include <video/artinchip_ge.h>

#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#endif

#include "../mpp_func.h"
#include "ge_hw.h"
#include "ge_reg.h"

#define AIC_GE_NAME "ge"

#define MAX_WIDTH 4096
#define MAX_HEIGHT 4096

#define GE_TIMEOUT_MS 1000

#define BYTE_ALIGN(x, byte) (((x) + ((byte) - 1))&(~((byte) - 1)))

#ifdef CONFIG_DMA_SHARED_BUFFER
struct ge_dmabuf {
	int                       fd[3];
	struct dma_buf            *buf[3];
	struct dma_buf_attachment *attach[3];
	struct sg_table           *sgt[3];
	dma_addr_t                addr[3];
};
#endif

struct aic_ge_data {
	struct device        *dev;
	void __iomem         *regs;
	struct reset_control *reset;
	struct clk           *mclk;
	ulong                mclk_rate;
	int                  irq;
	struct mutex         lock; /* hardware resource lock */
	int                  refs;
	wait_queue_head_t    wait;
	u32                  status;
	struct ge_dmabuf     src_dma_buf;
	struct ge_dmabuf     dst_dma_buf;
	u32                  src_premul_en;
	u32                  src_de_premul_en;
	u32                  dst_de_premul_en;
	u32                  out_premul_en;
	u32                  src_alpha_coef;
	u32                  dst_alpha_coef;
	u32                  csc0_en;
	u32                  csc1_en;
	u32                  csc2_en;
	u32		     *dither_buff;
	dma_addr_t	     dither_dma_addr;
	bool                 blend_is_rgb;
	bool                 enable_dma_buf;
	enum ge_mode         ge_mode;
};

struct aic_ge_data *g_data;

#ifdef CONFIG_DMA_SHARED_BUFFER
static int ge_plane_num(enum mpp_pixel_format format)
{
	switch (format) {
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
		return 2;
	case MPP_FMT_YUV420P:
	case MPP_FMT_YUV422P:
	case MPP_FMT_YUV444P:
		return 3;
	default:
		break;
	}

	return 1;
}

static int aic_ge_put_dmabuf(struct device *dev,
			     struct mpp_buf *video_buf,
			     struct ge_dmabuf *dma_buf,
			     enum dma_data_direction direction)
{
	int plane_num;
	int i;

	if (video_buf->buf_type != MPP_DMA_BUF_FD)
		return 0;

	plane_num = ge_plane_num(video_buf->format);

	for (i = 0; i < plane_num; i++) {
		if (dma_buf->fd[i] < 0)
			break;

		dma_buf_unmap_attachment(dma_buf->attach[i],
					 dma_buf->sgt[i], direction);
		dma_buf_detach(dma_buf->buf[i], dma_buf->attach[i]);
		dma_buf_put(dma_buf->buf[i]);

		dma_buf->buf[i] = NULL;
		dma_buf->attach[i] = NULL;
		dma_buf->sgt[i] = NULL;
		dma_buf->addr[i] = 0;
		dma_buf->fd[i] = -1;
	}

	return 0;
}

static int aic_ge_get_dmabuf(struct device *dev,
			     struct mpp_buf *video_buf,
			     struct ge_dmabuf *dma_buf,
			     enum dma_data_direction direction)
{
	int plane_num;
	int i;
	int ret;

	if (video_buf->buf_type != MPP_DMA_BUF_FD)
		return 0;

	plane_num = ge_plane_num(video_buf->format);

	for (i = 0; i < plane_num; i++) {
		dma_buf->fd[i] = video_buf->fd[i];
		dma_buf->buf[i] = dma_buf_get(dma_buf->fd[i]);

		if (IS_ERR(dma_buf->buf[i])) {
			dev_err(dev, "dma_buf_get(%d) failed\n",
				dma_buf->fd[i]);
			ret = PTR_ERR(dma_buf->buf[i]);
			dma_buf->fd[i] = -1;
			goto end;
		}

		dma_buf->attach[i] = dma_buf_attach(dma_buf->buf[i], dev);
		if (IS_ERR(dma_buf->attach[i])) {
			dev_err(dev, "dma_buf_attach(%d) failed\n",
				dma_buf->fd[i]);
			dma_buf_put(dma_buf->buf[i]);
			ret = PTR_ERR(dma_buf->attach[i]);
			dma_buf->fd[i] = -1;
			goto end;
		}

		dma_buf->sgt[i] = dma_buf_map_attachment(dma_buf->attach[i],
							 direction);

		if (IS_ERR(dma_buf->sgt[i])) {
			dev_err(dev,
				"dma_buf_map_attachment(%d) failed\n",
				dma_buf->fd[i]);
			dma_buf_detach(dma_buf->buf[i], dma_buf->attach[i]);
			dma_buf_put(dma_buf->buf[i]);
			ret = PTR_ERR(dma_buf->sgt[i]);
			dma_buf->fd[i] = -1;
			goto end;
		}

		dma_buf->addr[i] = sg_dma_address(dma_buf->sgt[i]->sgl);

		video_buf->phy_addr[i] = dma_buf->addr[i];
	}

	return 0;
end:
	aic_ge_put_dmabuf(dev, video_buf, dma_buf, direction);
	return ret;
}

static int aic_ge_alloc_dither_dmabuf(struct device *dev, struct mpp_buf *video_buf, struct ge_ctrl *ctrl)
{
	u32 len = 0;
	u32 *dma_virt_addr = NULL;

	if (!ctrl->dither_en) {
		return 0;
	}

	len = BYTE_ALIGN(video_buf->size.width, 128) * 4;

	dma_virt_addr = dma_alloc_coherent(dev, len, &g_data->dither_dma_addr, GFP_KERNEL);
	if (IS_ERR(dma_virt_addr)) {
		dev_err(dev, "failed to allocate DMA memory\n");
		return -EINVAL;
	}

	g_data->dither_buff = dma_virt_addr;

	return 0;
}

static int aic_ge_free_dither_dmabuf(struct device *dev, struct mpp_buf *video_buf, struct ge_ctrl *ctrl)
{
	u32 len = 0;
	u32 *dma_virt_addr = 0;

	if (!ctrl->dither_en) {
		return 0;
	}

	dma_virt_addr = g_data->dither_buff;
	if (!dma_virt_addr) {
		len = BYTE_ALIGN(video_buf->size.width, 128) * 4;
		dma_free_coherent(dev, len, dma_virt_addr, g_data->dither_dma_addr);
	}

	return 0;
}
#endif

static inline int ge_check_buf(struct aic_ge_data   *data,
			       struct mpp_buf *video_buf)
{
	if (video_buf->buf_type == MPP_DMA_BUF_FD &&
	    !data->enable_dma_buf) {
		dev_err(data->dev, "unsupported buf type:%d\n",
			video_buf->buf_type);
		return -EINVAL;
	}

	return 0;
}

static irqreturn_t aic_ge_handler(int irq, void *ctx)
{
	struct aic_ge_data *data = (struct aic_ge_data *)ctx;

	data->status = ge_read_status(data->regs);
	ge_clear_status(data->regs, data->status);

	wake_up(&data->wait);
	return IRQ_HANDLED;
}

static int aic_ge_request_irq(struct device *dev,
			      struct aic_ge_data *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	int irq, ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "Couldn't get disp engine interrupt\n");
		return irq;
	}

	data->irq = irq;
	ret = devm_request_irq(dev, irq, aic_ge_handler, 0,
			       dev_name(dev), data);
	if (ret) {
		dev_err(dev, "Couldn't request the IRQ\n");
		return ret;
	}

	return 0;
}

static void ge_reset(struct aic_ge_data *data)
{
	reset_control_assert(data->reset);
	reset_control_deassert(data->reset);
}

static int ge_start_and_wait(struct aic_ge_data *data)
{
	data->status = 0;

	ge_enable_interrupt(data->regs);
	ge_start(data->regs);

	wait_event_timeout(data->wait, data->status,
			   msecs_to_jiffies(GE_TIMEOUT_MS));

	ge_disable_interrupt(data->regs);

	if (data->status == 0) {
		dev_err(data->dev, "ge timeout\n");
		ge_reset(data);
		return -ETIMEDOUT;
	} else if ((data->status & GE_CTRL_FINISH_IRQ_EN) == 0) {
		dev_err(data->dev, "ge error status:%08x\n", data->status);
		return -EINVAL;
	}

	return 0;
}

static inline bool is_rgb(enum mpp_pixel_format format)
{
	switch (format) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_XBGR_8888:
	case MPP_FMT_RGBX_8888:
	case MPP_FMT_BGRX_8888:
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_ABGR_1555:
	case MPP_FMT_RGBA_5551:
	case MPP_FMT_BGRA_5551:
	case MPP_FMT_RGB_565:
	case MPP_FMT_BGR_565:
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ABGR_4444:
	case MPP_FMT_RGBA_4444:
	case MPP_FMT_BGRA_4444:
		return true;
	default:
		break;
	}
	return false;
}

static inline bool need_blend(struct ge_ctrl *ctrl)
{
	if (ctrl->alpha_en || ctrl->ck_en)
		return true;
	else
		return false;
}

static void set_csc_flow(struct aic_ge_data *data,
			 struct ge_ctrl *ctrl,
			 enum mpp_pixel_format src_format,
			 enum mpp_pixel_format dst_format)
{
	bool src_is_rgb = is_rgb(src_format);
	bool dst_is_rgb = is_rgb(dst_format);
	bool is_blending = need_blend(ctrl);

	data->blend_is_rgb = true;

	if (!src_is_rgb &&
	    (is_blending || dst_is_rgb)) {
		data->csc0_en = 1;
	} else if (!src_is_rgb) {
		data->blend_is_rgb = false;
		data->csc0_en = 0;
	} else {
		data->csc0_en = 0;
	}

	if (is_blending && !dst_is_rgb)
		data->csc1_en = 1;
	else
		data->csc1_en = 0;

	if (data->blend_is_rgb && !dst_is_rgb)
		data->csc2_en = 1;
	else
		data->csc2_en = 0;
}

static void set_alpha_rules(struct aic_ge_data *data,
			    enum ge_pd_rules rules)
{
	switch (rules) {
	case GE_PD_NONE:
		data->src_alpha_coef = 2;
		data->dst_alpha_coef = 3;
		break;
	case GE_PD_CLEAR:
		data->src_alpha_coef = 0;
		data->dst_alpha_coef = 0;
		break;
	case GE_PD_SRC:
		data->src_alpha_coef = 1;
		data->dst_alpha_coef = 0;
		break;
	case GE_PD_SRC_OVER:
		data->src_alpha_coef = 1;
		data->dst_alpha_coef = 3;
		break;
	case GE_PD_DST_OVER:
		data->src_alpha_coef = 5;
		data->dst_alpha_coef = 1;
		break;
	case GE_PD_SRC_IN:
		data->src_alpha_coef = 4;
		data->dst_alpha_coef = 0;
		break;
	case GE_PD_DST_IN:
		data->src_alpha_coef = 0;
		data->dst_alpha_coef = 2;
		break;
	case GE_PD_SRC_OUT:
		data->src_alpha_coef = 5;
		data->dst_alpha_coef = 0;
		break;
	case GE_PD_DST_OUT:
		data->src_alpha_coef = 0;
		data->dst_alpha_coef = 3;
		break;
	case GE_PD_SRC_ATOP:
		data->src_alpha_coef = 4;
		data->dst_alpha_coef = 3;
		break;
	case GE_PD_DST_ATOP:
		data->src_alpha_coef = 5;
		data->dst_alpha_coef = 2;
		break;
	case GE_PD_ADD:
		data->src_alpha_coef = 1;
		data->dst_alpha_coef = 1;
		break;
	case GE_PD_XOR:
		data->src_alpha_coef = 5;
		data->dst_alpha_coef = 3;
		break;
	case GE_PD_DST:
		data->src_alpha_coef = 0;
		data->dst_alpha_coef = 1;
		break;
	default:
		data->src_alpha_coef = 2;
		data->dst_alpha_coef = 3;
		break;
	}
}

/* must call set_premuliply after set_alpha_rules */
int set_premuliply(struct aic_ge_data *data,
		   enum mpp_pixel_format src_format,
		   enum mpp_pixel_format dst_format,
		   int src_premul,
		   int dst_premul,
		   int is_fill_color,
		   struct ge_ctrl *ctrl)
{
	if (src_format > MPP_FMT_BGRA_4444)
		src_premul = 0;

	if (dst_format > MPP_FMT_BGRA_4444)
		dst_premul = 0;

	if (src_premul == 0 && dst_premul == 0) {
		data->src_premul_en = 0;
		data->src_de_premul_en = 0;
		data->dst_de_premul_en = 0;
		data->out_premul_en = 0;

		if (is_fill_color == 0 &&
		    ctrl->src_alpha_mode == 0 &&
		    ctrl->alpha_en &&
		    ctrl->ck_en == 0 &&
		    data->src_alpha_coef == 2 &&
		    data->dst_alpha_coef == 3) {
			data->src_premul_en = 1;
			data->src_alpha_coef = 1;
		}
	} else if (src_premul == 1 && dst_premul == 0) {
		data->src_premul_en = 0;
		data->src_de_premul_en = 1;
		data->dst_de_premul_en = 0;
		data->out_premul_en = 0;

		if (ctrl->src_alpha_mode == 0 &&
		    ctrl->alpha_en &&
		    ctrl->ck_en == 0 &&
		    data->src_alpha_coef == 2 &&
		    data->dst_alpha_coef == 3) {
			data->src_de_premul_en = 0;
			data->src_alpha_coef = 1;
		}
	} else if (src_premul == 1 && dst_premul == 1) {
		data->src_premul_en = 0;
		data->src_de_premul_en = 1;
		data->dst_de_premul_en = 1;
		data->out_premul_en = 1;
	} else if (src_premul == 0 && dst_premul == 1) {
		data->src_premul_en = 0;
		data->src_de_premul_en = 0;
		data->dst_de_premul_en = 1;
		data->out_premul_en = 1;
	}

	return 0;
}

static int check_bitblt(struct aic_ge_data *data, struct ge_bitblt *blt)
{
	enum mpp_pixel_format src_format = blt->src_buf.format;
	enum mpp_pixel_format dst_format = blt->dst_buf.format;
	struct mpp_rect *src_rect = &blt->src_buf.crop;
	struct mpp_rect *dst_rect = &blt->dst_buf.crop;
	struct mpp_size *src_size = &blt->src_buf.size;
	struct mpp_size *dst_size = &blt->dst_buf.size;
	unsigned int scan_order = MPP_SCAN_ORDER_GET(blt->ctrl.flags);
	unsigned int rot0_degree = MPP_ROTATION_GET(blt->ctrl.flags);

	if (scan_order) {
		if (rot0_degree) {
			dev_err(data->dev, "scan order unsupport rot0\n");
			return -EINVAL;
		}
		if (blt->ctrl.dither_en) {
			dev_err(data->dev, "scan order unsupport dither\n");
			return -EINVAL;
		}
		if (!is_rgb(src_format) || !is_rgb(dst_format)) {
			dev_err(data->dev, "scan order just support rgb format\n");
			return -EINVAL;
		}
	}

	if (blt->ctrl.dither_en) {
		if (!is_rgb(dst_format)) {
			dev_err(data->dev, "invalid dst format with the dither func on\n");
			return -EINVAL;
		}
	}

	if (blt->src_buf.crop_en) {
		if (src_rect->x < 0 ||
		    src_rect->y < 0 ||
		    src_rect->x >= src_size->width ||
		    src_rect->y >= src_size->height) {
			dev_err(data->dev, "%s failed, invalid src crop\n",
				__func__);
			return -EINVAL;
		}
	}

	if (blt->dst_buf.crop_en) {
		if (dst_rect->x < 0 ||
		    dst_rect->y < 0 ||
		    dst_rect->x >= dst_size->width ||
		    dst_rect->y >= dst_size->height) {
			dev_err(data->dev, "%s failed, invalid dst crop\n",
				__func__);
			return -EINVAL;
		}
	}

	if (!blt->src_buf.crop_en) {
		src_rect->x = 0;
		src_rect->y = 0;
		src_rect->width = src_size->width;
		src_rect->height = src_size->height;
	}

	if (!blt->dst_buf.crop_en) {
		dst_rect->x = 0;
		dst_rect->y = 0;
		dst_rect->width = dst_size->width;
		dst_rect->height = dst_size->height;
	}

	switch (src_format) {
	case MPP_FMT_YUV420P:
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
		src_rect->x = src_rect->x & (~1);
		src_rect->y = src_rect->y & (~1);
		src_rect->width = src_rect->width & (~1);
		src_rect->height = src_rect->height & (~1);
		src_size->width = src_size->width & (~1);
		src_size->height = src_size->height & (~1);
		break;
	case MPP_FMT_YUV422P:
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		src_rect->x = src_rect->x & (~1);
		src_rect->width = src_rect->width & (~1);
		src_size->width = src_size->width & (~1);
		break;
	default:
		break;
	}

	switch (dst_format) {
	case MPP_FMT_YUV420P:
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
		dst_rect->x = dst_rect->x & (~1);
		dst_rect->y = dst_rect->y & (~1);
		dst_rect->width = dst_rect->width & (~1);
		dst_rect->height = dst_rect->height & (~1);
		dst_size->width = dst_size->width & (~1);
		dst_size->height = dst_size->height & (~1);
		break;
	case MPP_FMT_YUV422P:
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		dst_rect->x = dst_rect->x & (~1);
		dst_rect->width = dst_rect->width & (~1);
		dst_size->width = dst_size->width & (~1);
		break;
	default:
		break;
	}

	/* crop src invalid region */
	if ((src_rect->x + src_rect->width) > src_size->width)
		src_rect->width = src_size->width - src_rect->x;

	if ((src_rect->y + src_rect->height) > src_size->height)
		src_rect->height = src_size->height - src_rect->y;

	/* crop dst invalid region */
	if ((dst_rect->x + dst_rect->width) > dst_size->width)
		dst_rect->width = dst_size->width - dst_rect->x;

	if ((dst_rect->y + dst_rect->height) > dst_size->height)
		dst_rect->height = dst_size->height - dst_rect->y;

	if (src_rect->height > MAX_HEIGHT ||
			 src_rect->width > MAX_WIDTH) {
		dev_err(data->dev, "invalid src size, over the largest\n");
		return -EINVAL;
	}

	if (dst_rect->height > MAX_HEIGHT ||
			 dst_rect->width > MAX_WIDTH) {
		dev_err(data->dev, "invalid dst size, over the largest\n");
		return -EINVAL;
	}

	if (!is_rgb(src_format) &&
			(src_rect->width < 8 || src_rect->height < 8))  {
		dev_err(data->dev,
			"invalid src size, the min size of yuv is 8x8\n");

		return -EINVAL;
	}

	if (!is_rgb(dst_format) &&
			(dst_rect->width < 8 || dst_rect->height < 8))  {
		dev_err(data->dev,
			"invalid dst size, the min size of yuv is 8x8\n");

		return -EINVAL;;
	}

	return 0;
}

static void set_alpha_rules_and_premul(struct aic_ge_data *data,
				       struct ge_ctrl *ctrl,
				       enum mpp_pixel_format src_format,
				       enum mpp_pixel_format dst_format,
				       u32 src_buf_flags,
				       u32 dst_buf_flags,
				       int is_fill_color)
{
	if (ctrl->alpha_en)
		set_alpha_rules(data, ctrl->alpha_rules);

	set_premuliply(data, src_format, dst_format,
		       MPP_BUF_PREMULTIPLY_GET(src_buf_flags),
		       MPP_BUF_PREMULTIPLY_GET(dst_buf_flags),
		       is_fill_color, ctrl);
}

static int ge_config_scaler(struct aic_ge_data *data,
			    struct ge_bitblt *blt)
{
	enum mpp_pixel_format format;
	int in_w[2];
	int in_h[2];
	int out_w;
	int out_h;
	int channel_num;
	int scaler_en;
	int rot0_degree;
	int i;
	int dx[2];
	int dy[2];
	int h_phase[2];
	int v_phase[2];

	channel_num = 1;
	scaler_en = 1;
	format = blt->src_buf.format;
	rot0_degree = MPP_ROTATION_GET(blt->ctrl.flags);

	in_w[0] = blt->src_buf.crop.width;
	in_h[0] = blt->src_buf.crop.height;

	if (rot0_degree == MPP_ROTATION_90 ||
	    rot0_degree == MPP_ROTATION_270) {
		out_w = blt->dst_buf.crop.height;
		out_h = blt->dst_buf.crop.width;
	} else {
		out_w = blt->dst_buf.crop.width;
		out_h = blt->dst_buf.crop.height;
	}

	switch (format) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_XBGR_8888:
	case MPP_FMT_RGBX_8888:
	case MPP_FMT_BGRX_8888:
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_ABGR_1555:
	case MPP_FMT_RGBA_5551:
	case MPP_FMT_BGRA_5551:
	case MPP_FMT_RGB_565:
	case MPP_FMT_BGR_565:
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ABGR_4444:
	case MPP_FMT_RGBA_4444:
	case MPP_FMT_BGRA_4444:
		if (in_w[0] == out_w && in_h[0] == out_h)
			scaler_en = 0;
		else {
			dx[0] = (in_w[0] << 16) / out_w;
			dy[0] = (in_h[0] << 16) / out_h;
			h_phase[0] = dx[0] >> 1;
			v_phase[0] = dy[0] >> 1;
		}
		break;
	case MPP_FMT_YUV400:
		dx[0] = (in_w[0] << 16) / out_w;
		dy[0] = (in_h[0] << 16) / out_h;
		h_phase[0] = dx[0] >> 1;
		v_phase[0] = dy[0] >> 1;
		break;
	case MPP_FMT_YUV420P:
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
		channel_num = 2;
		in_w[1] = in_w[0] >> 1;
		in_h[1] = in_h[0] >> 1;

		dx[0] = (in_w[0] << 16) / out_w;
		dy[0] = (in_h[0] << 16) / out_h;
		h_phase[0] = dx[0] >> 1;
		v_phase[0] = dy[0] >> 1;

		dx[0] = dx[0] & (~1);
		dy[0] = dy[0] & (~1);
		h_phase[0] = h_phase[0] & (~1);
		v_phase[0] = v_phase[0] & (~1);

		/* change init phase */
		if (((dx[0] - h_phase[0]) >> 16) > 4) {
			h_phase[0] += (((dx[0] - h_phase[0]) >> 16) - 4) << 16;
		}

		if (((dy[0] - v_phase[0]) >> 16) > 3) {
			v_phase[0] += (((dy[0] - v_phase[0]) >> 16) - 4) << 16;
		}

		dx[1] = dx[0] >> 1;
		dy[1] = dy[0] >> 1;
		h_phase[1] = h_phase[0] >> 1;
		v_phase[1] = v_phase[0] >> 1;
		break;
	case MPP_FMT_YUV422P:
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		channel_num = 2;

		in_w[1] = in_w[0] >> 1;
		in_h[1] = in_h[0];

		dx[0] = (in_w[0] << 16) / out_w;
		dy[0] = (in_h[0] << 16) / out_h;
		h_phase[0] = dx[0] >> 1;
		v_phase[0] = dy[0] >> 1;

		dx[0] = dx[0] & (~1);
		h_phase[0] = h_phase[0] & (~1);

		/* change init phase */
		if (((dx[0] - h_phase[0]) >> 16) > 4) {
			h_phase[0] += (((dx[0] - h_phase[0]) >> 16) - 4) << 16;
		}

		dx[1] = dx[0] >> 1;
		dy[1] = dy[0];
		h_phase[1] = h_phase[0] >> 1;
		v_phase[1] = v_phase[0];
		break;
	case MPP_FMT_YUV444P:
		channel_num = 2;
		in_w[1] = in_w[0];
		in_h[1] = in_h[0];

		dx[0] = (in_w[0] << 16) / out_w;
		dy[0] = (in_h[0] << 16) / out_h;
		h_phase[0] = dx[0] >> 1;
		v_phase[0] = dy[0] >> 1;

		dx[1] = dx[0];
		dy[1] = dy[0];
		h_phase[1] = h_phase[0];
		v_phase[1] = v_phase[0];
		break;
	default:
		scaler_en = 0;
		dev_err(data->dev, "invalid format: %d\n", format);
		return -EINVAL;
	}

	if (scaler_en) {
		if (is_rgb(format) &&
				(in_w[0] < 4 || in_h[0] < 4)) {
			dev_err(data->dev,
				"the min size of rgb is 4x4, when scaler enable\n");
			return -EINVAL;
		}

		if (is_rgb(blt->dst_buf.format) &&
				(out_h < 4 || out_w < 4)) {
			dev_err(data->dev,
				"the min size of rgb is 4x4, when scaler enable\n");
			return -EINVAL;
		}

		for (i = 0; i < channel_num; i++) {
			ge_set_scaler0(data->regs, in_w[i], in_h[i],
				       out_w, out_h,
				       dx[i], dy[i],
				       h_phase[i], v_phase[i],
				       i);
		}
		ge_scaler0_enable(data->regs, 1);
	} else {
		ge_scaler0_enable(data->regs, 0);
	}

	return 0;
}

/*
 *@addr[]: in/out addr
 *
 */
static int ge_buf_crop(u32 addr[], u32 stride[],
		       enum mpp_pixel_format format,
		       u32 x_offset,
		       u32 y_offset,
		       u32 width,
		       u32 height)
{
	int offset;

	switch (format) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_XBGR_8888:
	case MPP_FMT_RGBX_8888:
	case MPP_FMT_BGRX_8888:
		addr[0] += x_offset * 4 + y_offset * stride[0];
		break;
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
		addr[0] += x_offset * 3 + y_offset * stride[0];
		break;
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_ABGR_1555:
	case MPP_FMT_RGBA_5551:
	case MPP_FMT_BGRA_5551:
	case MPP_FMT_RGB_565:
	case MPP_FMT_BGR_565:
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ABGR_4444:
	case MPP_FMT_RGBA_4444:
	case MPP_FMT_BGRA_4444:
		addr[0] += x_offset * 2 + y_offset * stride[0];
		break;
	case MPP_FMT_YUV420P:
		addr[0] += x_offset + y_offset * stride[0];
		offset = (x_offset >> 1) + (y_offset >> 1) * stride[1];
		addr[1] += offset;
		addr[2] += offset;
		break;
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
		addr[0] += x_offset + y_offset * stride[0];
		addr[1] += x_offset + (y_offset >> 1) * stride[1];
		break;
	case MPP_FMT_YUV400:
		addr[0] += x_offset + y_offset * stride[0];
		break;
	case MPP_FMT_YUV422P:
		addr[0] += x_offset + y_offset * stride[0];
		offset = (x_offset >> 1) + y_offset * stride[1];
		addr[1] += offset;
		addr[2] += offset;
		break;
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
		addr[0] += x_offset + y_offset * stride[0];
		addr[1] += x_offset + y_offset * stride[1];
		break;
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		addr[0] += (x_offset << 1) + y_offset * stride[0];
		break;
	case MPP_FMT_YUV444P:
		addr[0] += x_offset + y_offset * stride[0];
		addr[1] += x_offset + y_offset * stride[1];
		addr[2] += x_offset + y_offset * stride[1];
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ge_config_addr(struct aic_ge_data     *data,
			  struct mpp_buf   *src_buf,
			  struct mpp_buf   *dst_buf,
			  struct ge_ctrl     *ctrl)
{
	u32 src_w;
	u32 src_h;
	u32 dst_w;
	u32 dst_h;
	u32 src_addr[3];
	u32 dst_addr[3];
	u32 src_stride[2];
	u32 dst_stride[2];
	struct mpp_rect *src_rect;
	struct mpp_rect *dst_rect;

	src_rect = &src_buf->crop;
	dst_rect = &dst_buf->crop;

	src_w = src_rect->width;
	src_h = src_rect->height;
	dst_w = dst_rect->width;
	dst_h = dst_rect->height;

	src_addr[0] = src_buf->phy_addr[0];
	src_addr[1] = src_buf->phy_addr[1];
	src_addr[2] = src_buf->phy_addr[2];

	dst_addr[0] = dst_buf->phy_addr[0];
	dst_addr[1] = dst_buf->phy_addr[1];
	dst_addr[2] = dst_buf->phy_addr[2];

	src_stride[0] = src_buf->stride[0];
	src_stride[1] = src_buf->stride[1];
	dst_stride[0] = dst_buf->stride[0];
	dst_stride[1] = dst_buf->stride[1];

	ge_buf_crop(src_addr, src_stride,
		    src_buf->format,
		    src_rect->x,
		    src_rect->y,
		    src_w,
		    src_h);

	ge_buf_crop(dst_addr, dst_stride,
		    dst_buf->format,
		    dst_rect->x,
		    dst_rect->y,
		    dst_w,
		    dst_h);

	ge_set_src_info(data->regs, src_w, src_h,
			src_stride[0], src_stride[1],
			src_addr);

	ge_set_output_info(data->regs, dst_w, dst_h,
			   dst_stride[0], dst_stride[1],
			   dst_addr);

	if (need_blend(ctrl)) {
		ge_set_dst_info(data->regs, dst_w, dst_h,
				dst_stride[0], dst_stride[1],
				dst_addr);
	}

	return 0;
}

static int check_fillrect(struct aic_ge_data *data,
			  struct ge_fillrect *fill)
{
	enum mpp_pixel_format dst_format = fill->dst_buf.format;
	struct mpp_rect *dst_rect = &fill->dst_buf.crop;
	struct mpp_size *dst_size = &fill->dst_buf.size;

	if (fill->dst_buf.crop_en) {
		if (dst_rect->x < 0 ||
		    dst_rect->y < 0 ||
		    dst_size->width <= 0  ||
		    dst_size->height <= 0 ||
		    dst_rect->x >= dst_size->width ||
		    dst_rect->y >= dst_size->height) {
			dev_err(data->dev, "%s failed\n", __func__);
			return -EINVAL;
		}
	}

	switch (fill->type) {
	case GE_NO_GRADIENT:
	case GE_H_LINEAR_GRADIENT:
	case GE_V_LINEAR_GRADIENT:
		break;
	default:
		dev_err(data->dev, "invalid type: %08x\n", fill->type);
		return -EINVAL;
	}

	if (!fill->dst_buf.crop_en) {
		dst_rect->x = 0;
		dst_rect->y = 0;
		dst_rect->width = dst_size->width;
		dst_rect->height = dst_size->height;
	}

	switch (dst_format) {
	case MPP_FMT_YUV420P:
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
		dst_rect->x = dst_rect->x & (~1);
		dst_rect->y = dst_rect->y & (~1);
		dst_rect->width = dst_rect->width & (~1);
		dst_rect->height = dst_rect->height & (~1);
		dst_size->width = dst_size->width & (~1);
		dst_size->height = dst_size->height & (~1);
		break;
	case MPP_FMT_YUV422P:
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		dst_rect->x = dst_rect->x & (~1);
		dst_rect->width = dst_rect->width & (~1);
		dst_size->width = dst_size->width & (~1);
		break;
	default:
		break;
	}

	/* crop dst invalid region */
	if ((dst_rect->x + dst_rect->width) > dst_size->width)
		dst_rect->width = dst_size->width - dst_rect->x;

	if ((dst_rect->y + dst_rect->height) > dst_size->height)
		dst_rect->height = dst_size->height - dst_rect->y;

	if (dst_rect->width > MAX_WIDTH ||
		dst_rect->height > MAX_HEIGHT) {
		dev_err(data->dev, "invalid dst size, over the largest\n");
		return -EINVAL;
	}

	if (!is_rgb(dst_format) &&
			(dst_rect->width < 8 ||
			 dst_rect->height < 8)) {
		dev_err(data->dev,
			"invalid dst nsize, the min size of yuv is 8\n");
		return -EINVAL;
	}

	return 0;
}

static int ge_config_fillrect_addr(struct aic_ge_data *data,
				   struct ge_fillrect *fill)
{
	u32 dst_w;
	u32 dst_h;
	u32 dst_addr[3];
	u32 dst_stride[2];
	struct mpp_rect *dst_rect;

	dst_rect = &fill->dst_buf.crop;
	dst_w = dst_rect->width;
	dst_h = dst_rect->height;

	dst_addr[0] = fill->dst_buf.phy_addr[0];
	dst_addr[1] = fill->dst_buf.phy_addr[1];
	dst_addr[2] = fill->dst_buf.phy_addr[2];

	dst_stride[0] = fill->dst_buf.stride[0];
	dst_stride[1] = fill->dst_buf.stride[1];

	ge_buf_crop(dst_addr, dst_stride,
		    fill->dst_buf.format,
		    dst_rect->x,
		    dst_rect->y,
		    dst_w,
		    dst_h);

	ge_set_output_info(data->regs, dst_w, dst_h,
			   dst_stride[0], dst_stride[1],
			   dst_addr);

	/* src must has the same width and height as dst in fillrect */
	ge_set_src_info(data->regs, dst_w, dst_h,
			dst_stride[0], dst_stride[1],
			dst_addr);

	if (need_blend(&fill->ctrl)) {
		ge_set_dst_info(data->regs, dst_w, dst_h,
				dst_stride[0], dst_stride[1],
				dst_addr);
	}

	return 0;
}

static int ge_fillrect(struct aic_ge_data *data,
		       struct ge_fillrect *fill)
{
	int ret;
	enum mpp_pixel_format src_fmt = MPP_FMT_ARGB_8888;

	/* check buf type */
	if (ge_check_buf(data, &fill->dst_buf) < 0)
		return -EINVAL;

	if (check_fillrect(data, fill) != 0)
		return -EINVAL;

#ifdef CONFIG_DMA_SHARED_BUFFER
	if (aic_ge_get_dmabuf(data->dev, &fill->dst_buf,
			      &data->dst_dma_buf, DMA_BIDIRECTIONAL)) {
		return -EINVAL;
	}
#endif

	set_alpha_rules_and_premul(data, &fill->ctrl,
				   src_fmt, fill->dst_buf.format,
				   0, fill->dst_buf.flags,
				   1);

	set_csc_flow(data, &fill->ctrl,
		     src_fmt, fill->dst_buf.format);

	/* config dst csc1 yuvtorgb coefs */
	if (data->csc1_en)
		ge_set_csc_coefs(data->regs,
				 MPP_BUF_COLOR_SPACE_GET(fill->dst_buf.flags),
				 1);

	/* config dst csc2 rgb2yuv coefs */
	if (data->csc2_en)
		ge_set_csc2_coefs(data->regs,
				  MPP_BUF_COLOR_SPACE_GET(fill->dst_buf.flags));

	ge_config_src_simple(data->regs,
			     fill->ctrl.src_global_alpha,
			     fill->ctrl.src_alpha_mode,
			     data->src_premul_en,
			     0, /* func_select */
			     src_fmt,
			     fill->type + 1); /* source_mode */

	ge_config_output_ctrl(data->regs,
			      data->out_premul_en,
			      fill->dst_buf.format,
			      fill->ctrl.dither_en,
			      data->csc2_en);

	if (need_blend(&fill->ctrl)) {
		ge_dst_enable(data->regs,
			      fill->ctrl.dst_global_alpha,
			      fill->ctrl.dst_alpha_mode,
			      fill->dst_buf.format,
			      data->csc1_en);

		ge_config_blend(data->regs,
				data->src_de_premul_en,
				data->dst_de_premul_en,
				0, /* disable alpha output oxff */
				data->src_alpha_coef,
				data->dst_alpha_coef,
				fill->ctrl.ck_en,
				fill->ctrl.alpha_en);

	} else {
		ge_config_blend(data->regs,
				data->src_de_premul_en,
				data->dst_de_premul_en,
				0, /* disable alpha output oxff */
				data->src_alpha_coef,
				data->dst_alpha_coef,
				fill->ctrl.ck_en,
				fill->ctrl.alpha_en);

		ge_dst_disable(data->regs);
	}

	if (fill->ctrl.ck_en)
		ge_config_color_key(data->regs, fill->ctrl.ck_value);

	ge_scaler0_enable(data->regs, 0);
	ge_config_fillrect_addr(data, fill);

	switch (fill->type) {
	case GE_NO_GRADIENT:
		ge_config_fillrect(data->regs, fill->start_color);
		break;
	case GE_H_LINEAR_GRADIENT:
		ge_config_fill_gradient(data->regs,
					fill->dst_buf.crop.width,
					fill->dst_buf.crop.height,
					fill->start_color,
					fill->end_color,
					0);
		break;
	case GE_V_LINEAR_GRADIENT:
		ge_config_fill_gradient(data->regs,
					fill->dst_buf.crop.width,
					fill->dst_buf.crop.height,
					fill->start_color,
					fill->end_color,
					1);
		break;
	default:
		break;
	}

	ret = ge_start_and_wait(data);

#ifdef CONFIG_DMA_SHARED_BUFFER
	aic_ge_put_dmabuf(data->dev,
			  &fill->dst_buf,
			  &data->dst_dma_buf,
			  DMA_BIDIRECTIONAL);
#endif
	return ret;
}

static int ge_base_bitblt(struct aic_ge_data *data, struct ge_bitblt *blt, u32 *csc_coef)
{
	int ret = 0;

	/* check buf type */
	if (ge_check_buf(data, &blt->src_buf) < 0 ||
	    ge_check_buf(data, &blt->dst_buf) < 0) {
		return -EINVAL;
	}

	if (check_bitblt(data, blt) != 0)
		return -EINVAL;

#ifdef CONFIG_DMA_SHARED_BUFFER
	if (aic_ge_get_dmabuf(data->dev, &blt->src_buf,
			      &data->src_dma_buf, DMA_TO_DEVICE)) {
		return -EINVAL;
	}

	if (aic_ge_get_dmabuf(data->dev, &blt->dst_buf,
			      &data->dst_dma_buf, DMA_BIDIRECTIONAL)) {
		aic_ge_put_dmabuf(data->dev,
				  &blt->src_buf,
				  &data->src_dma_buf,
				  DMA_TO_DEVICE);
		return -EINVAL;
	}

	if (aic_ge_alloc_dither_dmabuf(data->dev, &blt->src_buf, &blt->ctrl)) {
		return -EINVAL;
	}
#endif

	set_alpha_rules_and_premul(data, &blt->ctrl,
				   blt->src_buf.format, blt->dst_buf.format,
				   blt->src_buf.flags, blt->dst_buf.flags,
				   0);

	set_csc_flow(data, &blt->ctrl,
		     blt->src_buf.format, blt->dst_buf.format);

	/* config src csc0 yuvtorgb coefs */
	if (data->csc0_en)
		ge_set_csc_coefs(data->regs,
				 MPP_BUF_COLOR_SPACE_GET(blt->src_buf.flags),
				 0);

	if (csc_coef) {
		data->csc0_en = 1;
		ge_write_csc0_coefs(data->regs, csc_coef);
	}

	/* config dst csc1 yuvtorgb coefs */
	if (data->csc1_en)
		ge_set_csc_coefs(data->regs,
				 MPP_BUF_COLOR_SPACE_GET(blt->dst_buf.flags),
				 1);

	/* config dst csc2 rgb2yuv coefs */
	if (data->csc2_en)
		ge_set_csc2_coefs(data->regs,
				  MPP_BUF_COLOR_SPACE_GET(blt->dst_buf.flags));

	ge_config_src_ctrl(data->regs,
			   blt->ctrl.src_global_alpha,
			   blt->ctrl.src_alpha_mode,
			   data->src_premul_en,
			   MPP_SCAN_ORDER_GET(blt->ctrl.flags),
			   0, /* func_select */
			   blt->src_buf.format,
			   MPP_FLIP_V_GET(blt->ctrl.flags),
			   MPP_FLIP_H_GET(blt->ctrl.flags),
			   MPP_ROTATION_GET(blt->ctrl.flags),
			   0, /* fill buffer mode */
			   data->csc0_en);

	ge_config_output_ctrl(data->regs,
			      data->out_premul_en,
			      blt->dst_buf.format,
			      blt->ctrl.dither_en,
			      data->csc2_en);

	if (need_blend(&blt->ctrl)) {
		ge_dst_enable(data->regs,
			      blt->ctrl.dst_global_alpha,
			      blt->ctrl.dst_alpha_mode,
			      blt->dst_buf.format,
			      data->csc1_en);

		ge_config_blend(data->regs,
				data->src_de_premul_en,
				data->dst_de_premul_en,
				0, /* disable alpha output oxff */
				data->src_alpha_coef,
				data->dst_alpha_coef,
				blt->ctrl.ck_en,
				blt->ctrl.alpha_en);

	} else {
		ge_config_blend(data->regs,
				data->src_de_premul_en,
				data->dst_de_premul_en,
				0, /* disable alpha output oxff */
				data->src_alpha_coef,
				data->dst_alpha_coef,
				blt->ctrl.ck_en,
				blt->ctrl.alpha_en);

		ge_dst_disable(data->regs);
	}

	if (blt->ctrl.ck_en)
		ge_config_color_key(data->regs, blt->ctrl.ck_value);

	if (blt->ctrl.dither_en)
		ge_config_dither(data->regs, g_data->dither_dma_addr);

	ge_config_scaler(data, blt);

	ge_config_addr(data, &blt->src_buf, &blt->dst_buf, &blt->ctrl);

	ret = ge_start_and_wait(data);

#ifdef CONFIG_DMA_SHARED_BUFFER
	aic_ge_put_dmabuf(data->dev,
			  &blt->src_buf,
			  &data->src_dma_buf,
			  DMA_TO_DEVICE);

	aic_ge_put_dmabuf(data->dev,
			  &blt->dst_buf,
			  &data->dst_dma_buf,
			  DMA_BIDIRECTIONAL);

	aic_ge_free_dither_dmabuf(data->dev, &blt->src_buf, &blt->ctrl);
#endif
	return ret;
}

static int ge_bitblt(struct aic_ge_data *data, struct ge_bitblt *blt)
{
	return ge_base_bitblt(data, blt, NULL);
}

static int check_format_and_size(struct aic_ge_data *data,
				 struct mpp_buf *src_buf,
				 struct mpp_buf *dst_buf)
{
	enum mpp_pixel_format src_format = src_buf->format;
	enum mpp_pixel_format dst_format = dst_buf->format;
	struct mpp_rect *src_rect = &src_buf->crop;
	struct mpp_rect *dst_rect = &dst_buf->crop;

	if (src_buf->crop_en) {
		if (src_rect->x < 0 ||
		    src_rect->y < 0 ||
		    src_rect->x >= src_buf->size.width ||
		    src_rect->y >= src_buf->size.height) {
			dev_err(data->dev, "%s failed, invalid src crop\n",
				__func__);
			return -EINVAL;
		}
	}

	if (dst_buf->crop_en) {
		if (dst_rect->x < 0 ||
		    dst_rect->y < 0 ||
		    dst_rect->x >= dst_buf->size.width ||
		    dst_rect->y >= dst_buf->size.height) {
			dev_err(data->dev, "%s failed, invalid dst crop\n",
				__func__);
			return -EINVAL;
		}
	}

	if (!src_buf->crop_en) {
		src_rect->x = 0;
		src_rect->y = 0;
		src_rect->width = src_buf->size.width;
		src_rect->height = src_buf->size.height;
	}

	if (!dst_buf->crop_en) {
		dst_rect->x = 0;
		dst_rect->y = 0;
		dst_rect->width = dst_buf->size.width;
		dst_rect->height = dst_buf->size.height;
	}

	switch (src_format) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_XBGR_8888:
	case MPP_FMT_RGBX_8888:
	case MPP_FMT_BGRX_8888:
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_ABGR_1555:
	case MPP_FMT_RGBA_5551:
	case MPP_FMT_BGRA_5551:
	case MPP_FMT_RGB_565:
	case MPP_FMT_BGR_565:
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ABGR_4444:
	case MPP_FMT_RGBA_4444:
	case MPP_FMT_BGRA_4444:
		break;
	default:
		dev_err(data->dev,
			"unsupport src format:%d\n", src_format);
		return -EINVAL;
	}

	switch (dst_format) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_XBGR_8888:
	case MPP_FMT_RGBX_8888:
	case MPP_FMT_BGRX_8888:
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
	case MPP_FMT_ARGB_1555:
	case MPP_FMT_ABGR_1555:
	case MPP_FMT_RGBA_5551:
	case MPP_FMT_BGRA_5551:
	case MPP_FMT_RGB_565:
	case MPP_FMT_BGR_565:
	case MPP_FMT_ARGB_4444:
	case MPP_FMT_ABGR_4444:
	case MPP_FMT_RGBA_4444:
	case MPP_FMT_BGRA_4444:
		break;
	default:
		dev_err(data->dev, "unsupport dst format:%d\n", dst_format);
		return -EINVAL;
	}

	/* crop src invalid region */
	if ((src_rect->x + src_rect->width) >
	    src_buf->size.width)
		src_rect->width = src_buf->size.width -
				  src_rect->x;

	if ((src_rect->y + src_rect->height) >
	    src_buf->size.height)
		src_rect->height = src_buf->size.height -
				   src_rect->y;

	if (src_rect->width < 4 || src_rect->height < 4 ||
			src_rect->width > MAX_WIDTH ||
			src_rect->height > MAX_HEIGHT) {
		dev_err(data->dev, "unsupport src size\n");
		return -EINVAL;
	}

	/* crop dst invalid region */
	if ((dst_rect->x + dst_rect->width) >
	    dst_buf->size.width)
		dst_rect->width = dst_buf->size.width -
				  dst_rect->x;

	if ((dst_rect->y + dst_rect->height) >
	    dst_buf->size.height)
		dst_rect->height = dst_buf->size.height -
				   dst_rect->y;

	if (dst_rect->width < 4 || dst_rect->height < 4 ||
			dst_rect->width > MAX_WIDTH ||
			dst_rect->height > MAX_HEIGHT) {
		dev_err(data->dev, "unsupport dst size\n");
		return -EINVAL;
	}

	return 0;
}

static int ge_rotate(struct aic_ge_data *data, struct ge_rotation *rot)
{
	int ret;

	/* check buf type */
	if (ge_check_buf(data, &rot->src_buf) < 0 ||
	    ge_check_buf(data, &rot->dst_buf) < 0) {
		return -EINVAL;
	}

	if (check_format_and_size(data, &rot->src_buf,
				  &rot->dst_buf) != 0)
		return -EINVAL;

#ifdef CONFIG_DMA_SHARED_BUFFER
	if (aic_ge_get_dmabuf(data->dev, &rot->src_buf,
			      &data->src_dma_buf, DMA_TO_DEVICE)) {
		return -EINVAL;
	}

	if (aic_ge_get_dmabuf(data->dev, &rot->dst_buf,
			      &data->dst_dma_buf, DMA_BIDIRECTIONAL)) {
		aic_ge_put_dmabuf(data->dev,
				  &rot->src_buf,
				  &data->src_dma_buf,
				  DMA_TO_DEVICE);
		return -EINVAL;
	}
#endif

	set_alpha_rules_and_premul(data, &rot->ctrl,
				   rot->src_buf.format, rot->dst_buf.format,
				   rot->src_buf.flags, rot->dst_buf.flags,
				   0);

	/* rot1 only support rgb format */
	data->csc0_en = 0;
	data->csc1_en = 0;
	data->csc2_en = 0;

	ge_config_src_simple(data->regs,
			     rot->ctrl.src_global_alpha,
			     rot->ctrl.src_alpha_mode,
			     data->src_premul_en,
			     1, /* rot1 */
			     rot->src_buf.format,
			     0); /* fill buffer mode */

	ge_config_output_ctrl(data->regs,
			      data->out_premul_en,
			      rot->dst_buf.format,
			      0, /* rot1 does't support dither */
			      data->csc2_en);

	if (need_blend(&rot->ctrl)) {
		ge_dst_enable(data->regs,
			      rot->ctrl.dst_global_alpha,
			      rot->ctrl.dst_alpha_mode,
			      rot->dst_buf.format,
			      data->csc1_en);

		ge_config_blend(data->regs,
				data->src_de_premul_en,
				data->dst_de_premul_en,
				0, /* disable alpha output oxff */
				data->src_alpha_coef,
				data->dst_alpha_coef,
				rot->ctrl.ck_en,
				rot->ctrl.alpha_en);

	} else {
		ge_config_blend(data->regs,
				data->src_de_premul_en,
				data->dst_de_premul_en,
				0, /* disable alpha output oxff */
				data->src_alpha_coef,
				data->dst_alpha_coef,
				rot->ctrl.ck_en,
				rot->ctrl.alpha_en);

		ge_dst_disable(data->regs);
	}

	if (rot->ctrl.ck_en) {
		dev_warn(data->dev, "warning: rot does't support color key\n");
		rot->ctrl.ck_en = 0;
	}

	ge_scaler0_enable(data->regs, 0);

	ge_config_rot1(data->regs,
		       rot->angle_sin,
		       rot->angle_cos,
		       rot->src_rot_center.x,
		       rot->src_rot_center.y,
		       rot->dst_rot_center.x,
		       rot->dst_rot_center.y);

	ge_config_addr(data, &rot->src_buf, &rot->dst_buf, &rot->ctrl);

	ret = ge_start_and_wait(data);

#ifdef CONFIG_DMA_SHARED_BUFFER
	aic_ge_put_dmabuf(data->dev,
			  &rot->src_buf,
			  &data->src_dma_buf,
			  DMA_TO_DEVICE);

	aic_ge_put_dmabuf(data->dev,
			  &rot->dst_buf,
			  &data->dst_dma_buf,
			  DMA_BIDIRECTIONAL);
#endif
	return ret;
}

static long ge_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	mutex_lock(&g_data->lock);

	switch (cmd) {
	case IOC_GE_VERSION:
	{
		u32 version = ge_get_version_id(g_data->regs);

		if (copy_to_user((void __user *)arg, &version, sizeof(u32)))
			ret = -EFAULT;
	}
	break;
	case IOC_GE_MODE:
		if (copy_to_user((void __user *)arg, &g_data->ge_mode,
				 sizeof(enum ge_mode)))
			ret = -EFAULT;

		break;
	case IOC_GE_FILLRECT:
	{
		struct ge_fillrect fill;

		if (copy_from_user(&fill, (void __user *)arg, sizeof(fill))) {
			ret = -EFAULT;
			break;
		}
		ret = ge_fillrect(g_data, &fill);
	}
	break;
	case IOC_GE_BITBLT:
	{
		struct ge_bitblt blt;

		if (copy_from_user(&blt, (void __user *)arg, sizeof(blt))) {
			ret = -EFAULT;
			break;
		}
		ret = ge_bitblt(g_data, &blt);
	}
	break;
	case IOC_GE_ROTATE:
	{
		struct ge_rotation rot;

		if (copy_from_user(&rot, (void __user *)arg,
				   sizeof(rot))) {
			ret = -EFAULT;
			break;
		}
		ret = ge_rotate(g_data, &rot);
	}
	break;
	case IOC_GE_SYNC:
		dev_err(g_data->dev,
			"normal mode does't support cmd queue ioctl : %08x\n",
			cmd);

		ret = -EINVAL;
		break;
	default:
		dev_err(g_data->dev, "Invalid ioctl: %08x\n", cmd);
		ret = -EINVAL;
	}
	mutex_unlock(&g_data->lock);

	return ret;
}

static int ge_clk_enable(struct aic_ge_data *data)
{
	int ret;

	ret = clk_set_rate(data->mclk, data->mclk_rate);
	if (ret) {
		dev_err(data->dev, "Failed to set CLK_DE %ld\n",
			data->mclk_rate);
	}

	ret = reset_control_deassert(data->reset);
	if (ret) {
		dev_err(data->dev, "%s() - Couldn't deassert\n", __func__);
		return ret;
	}

	ret = clk_prepare_enable(data->mclk);
	if (ret) {
		dev_err(data->dev, "%s() - Couldn't enable mclk\n", __func__);
		return ret;
	}
	return 0;
}

static int ge_clk_disable(struct aic_ge_data *data)
{
	clk_disable_unprepare(data->mclk);
	reset_control_assert(data->reset);
	return 0;
}

static int ge_open(struct inode *inode, struct file *file)
{
	mutex_lock(&g_data->lock);
	if (g_data->refs == 0)
		ge_clk_enable(g_data);

	g_data->refs++;
	mutex_unlock(&g_data->lock);

	return nonseekable_open(inode, file);
}

static int ge_release(struct inode *inode, struct file *file)
{
	mutex_lock(&g_data->lock);
	if (g_data->refs == 1)
		ge_clk_disable(g_data);

	g_data->refs--;
	mutex_unlock(&g_data->lock);
	return 0;
}

static const struct file_operations ge_fops = {
	.owner          = THIS_MODULE,
	.open           = ge_open,
	.release        = ge_release,
	.unlocked_ioctl = ge_ioctl,
};

static struct miscdevice ge_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = AIC_GE_NAME,
	.fops  = &ge_fops,
};

static int aic_ge_parse_dt(struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;
	struct aic_ge_data *data = dev_get_drvdata(dev);

	ret = of_property_read_u32(np, "mclk", (u32 *)&data->mclk_rate);
	if (ret) {
		dev_err(dev, "Can't parse CLK_GE rate\n");
		return ret;
	}

	return 0;
}

int aic_ge_bitblt_with_hsbc(struct ge_bitblt *blt, u32 *csc_coef)
{
	int ret;

	mutex_lock(&g_data->lock);
	if (g_data->refs == 0) {
		ge_clk_enable(g_data);
		g_data->refs++;
	}

	ret = ge_base_bitblt(g_data, blt, csc_coef);

	mutex_unlock(&g_data->lock);

	return ret;
}
EXPORT_SYMBOL(aic_ge_bitblt_with_hsbc);

int aic_ge_bitblt(struct ge_bitblt *blt)
{
	return aic_ge_bitblt_with_hsbc(blt, NULL);
}
EXPORT_SYMBOL(aic_ge_bitblt);

static int aic_ge_probe(struct platform_device *pdev)
{
	int ret;
	struct aic_ge_data *data;
	struct resource *res;
	void __iomem *regs;

	dev_dbg(&pdev->dev, "%s()\n", __func__);

	ret = misc_register(&ge_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "misc_register failed:%d\n", ret);
		return ret;
	}

	data = devm_kzalloc(&pdev->dev,
			    sizeof(struct aic_ge_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	init_waitqueue_head(&data->wait);
	dev_set_drvdata(&pdev->dev, data);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	data->regs = regs;

	data->mclk = devm_clk_get(&pdev->dev, "ge");
	if (IS_ERR(data->mclk)) {
		dev_err(&pdev->dev, "Couldn't get ge clock\n");
		return PTR_ERR(data->mclk);
	}

	data->reset = devm_reset_control_get(&pdev->dev, "ge");
	if (IS_ERR(data->reset)) {
		dev_err(&pdev->dev, "Couldn't get reset\n");
		return PTR_ERR(data->reset);
	}

	ret = aic_ge_parse_dt(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't parse dt\n");
		return ret;
	}

	ret = aic_ge_request_irq(&pdev->dev, data);
	if (ret) {
		dev_err(&pdev->dev, "Couldn't request graphic engine IRQ\n");
		return ret;
	}

	data->ge_mode = GE_MODE_NORMAL;
	data->dev = &pdev->dev;
	mutex_init(&data->lock);

#ifdef CONFIG_DMA_SHARED_BUFFER
	data->enable_dma_buf = true;
#endif

	g_data = data;

	return 0;
}

static int aic_ge_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s()\n", __func__);
	misc_deregister(&ge_dev);
	return 0;
}

static const struct of_device_id aic_ge_match_table[] = {
	{.compatible = "artinchip,aic-ge-v1.0",},
	{},
};

MODULE_DEVICE_TABLE(of, aic_ge_match_table);

#ifdef CONFIG_PM
static int aic_ge_pm_suspend(struct device *dev)
{
	mutex_lock(&g_data->lock);
	if (g_data->refs > 0)
		ge_clk_disable(g_data);
	mutex_unlock(&g_data->lock);

	return 0;
}

static int aic_ge_pm_resume(struct device *dev)
{
	mutex_lock(&g_data->lock);
	if (g_data->refs > 0)
		ge_clk_enable(g_data);
	mutex_unlock(&g_data->lock);

	return 0;
}

static UNIVERSAL_DEV_PM_OPS(aic_ge_pm_ops, aic_ge_pm_suspend,
		aic_ge_pm_resume, NULL);
#endif
static struct platform_driver aic_ge_driver = {
	.probe = aic_ge_probe,
	.remove = aic_ge_remove,
	.driver = {
		.name = AIC_GE_NAME,
		.of_match_table = aic_ge_match_table,
#ifdef CONFIG_PM
		.pm = &aic_ge_pm_ops,
#endif
	},
};

module_platform_driver(aic_ge_driver);

MODULE_AUTHOR("Ning Fang <ning.fang@artinchip.com>");
MODULE_DESCRIPTION("Artinchip 2D Graphics Engine Driver");
MODULE_ALIAS("platform:" AIC_GE_NAME);
MODULE_LICENSE("GPL v2");

