// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/reset.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <dt-bindings/display/artinchip,aic-disp.h>

#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#endif

#include "aic_fb.h"
#include "video/artinchip_fb.h"
#include "hw/de_hw.h"

#define MAX_LAYER_NUM 2
#define MAX_RECT_NUM 4
#define RECT_NUM_SHIFT 2
#define CONFIG_NUM   (MAX_LAYER_NUM * MAX_RECT_NUM)

struct aic_de_dither {
	unsigned int enable;
	unsigned int red_bitdepth;
	unsigned int gleen_bitdepth;
	unsigned int blue_bitdepth;
};

struct aic_de_configs {
	const struct aicfb_layer_num *layer_num;
	const struct aicfb_layer_capability *cap;
};

#ifdef CONFIG_DMA_SHARED_BUFFER
struct aic_de_dmabuf {
	struct list_head list;
	struct dma_buf_info fds;
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	dma_addr_t phy_addr;
	int pid;
};
#endif

struct aic_de_comp {
	/* de_funcs must be the first member */
	struct de_funcs funcs;
	struct device *dev;
	struct videomode vm;
	bool vm_flag;
	bool init_flag;
	int vsync_flag;
	u32 scaler_active;
	wait_queue_head_t vsync_wait;
	spinlock_t slock;
	const struct aic_de_configs *config;
	struct aic_panel *panel;
	struct aic_de_dither dither;
	struct aicfb_layer_data layers[CONFIG_NUM];
	struct aicfb_alpha_config alpha[MAX_LAYER_NUM];
	struct aicfb_ck_config ck[MAX_LAYER_NUM];
#ifdef CONFIG_DMA_SHARED_BUFFER
	struct mutex mutex;
	struct list_head dma_buf;
#endif
	void __iomem *regs;
	struct reset_control *reset;
	struct clk *mclk;
	struct clk *pclk;
	int irq;
	ulong mclk_rate;
	struct aicfb_disp_prop disp_prop;
};
static struct aic_de_comp *g_aic_de_comp;

static struct aic_de_comp *aic_de_request_drvdata(void)
{
	return g_aic_de_comp;
}

static void aic_de_release_drvdata(void)
{

}

static irqreturn_t aic_de_handler(int irq, void *ctx)
{
	struct aic_de_comp *comp = ctx;
	unsigned int status;
	unsigned long flags;

	status = de_timing_interrupt_status(comp->regs);
	de_timing_interrupt_clean_status(comp->regs, status);

	if (status & TIMING_INIT_V_BLANK_FLAG) {
		spin_lock_irqsave(&comp->slock, flags);
		comp->vsync_flag = 1;
		spin_unlock_irqrestore(&comp->slock, flags);

		wake_up_interruptible(&comp->vsync_wait);
	}

	if (comp->scaler_active & SCALER0_CTRL_ACTIVE) {
		comp->scaler_active = comp->scaler_active & 0xF;

		de_scaler0_active_handle(comp->regs, comp->scaler_active);
	}

	if (status & TIMING_INIT_UNDERFLOW_FLAG)
		dev_err(comp->dev, "DE underflow error (irq status: %#x)\n",
				status);

	return IRQ_HANDLED;
}

static int aic_de_request_irq(struct device *dev,
			      struct aic_de_comp *comp)
{
	struct platform_device *pdev = to_platform_device(dev);
	int irq, ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "Couldn't get disp engine interrupt\n");
		return irq;
	}

	comp->irq = irq;

	ret = devm_request_irq(dev, irq, aic_de_handler, 0,
			       dev_name(dev), comp);
	if (ret) {
		dev_err(dev, "Couldn't request the IRQ\n");
		return ret;
	}

	return 0;
}

static inline bool is_valid_layer_id(struct aic_de_comp *comp, u32 layer_id)
{
	u32 total_num = comp->config->layer_num->vi_num
			+ comp->config->layer_num->ui_num;

	if (layer_id < total_num)
		return true;
	else
		return false;
}

static inline bool need_update_disp_prop(struct aic_de_comp *comp,
					 struct aicfb_disp_prop *disp_prop)
{
	if (disp_prop->bright != comp->disp_prop.bright ||
	    disp_prop->contrast != comp->disp_prop.contrast ||
	    disp_prop->saturation != comp->disp_prop.saturation ||
	    disp_prop->hue != comp->disp_prop.hue)
		return true;

	return false;
}

static inline bool need_use_hsbc(struct aic_de_comp *comp,
				 struct aicfb_disp_prop *disp_prop)
{
	if (disp_prop->bright != 50 ||
	    disp_prop->contrast != 50 ||
	    disp_prop->saturation != 50 ||
	    disp_prop->hue != 50)
		return true;

	return false;
}

static inline bool need_update_csc(struct aic_de_comp *comp, int color_space)
{
	/* get color space from video layer config */
	struct aicfb_layer_data *layer_data = &comp->layers[0];

	if (color_space != MPP_BUF_COLOR_SPACE_GET(layer_data->buf.flags))
		return true;

	return false;
}

static inline void de_check_scaler0_active(struct aic_de_comp *comp,
					u32 input_w, u32 input_h,
					u32 output_w, u32 output_h)
{
	int step = (input_h << 16) / output_h;
	u32 scaler_active = comp->scaler_active & 0xF;
	u32 index = 0;

	if (step <= 0x18000)
		index = 0;
	else if (step <= 0x20000)
		index = 1;
	else if (step <= 0x2C000)
		index = 2;
	else
		index = 3;

	if (scaler_active != index)
		comp->scaler_active = index | SCALER0_CTRL_ACTIVE;
}

static inline bool is_ui_layer(struct aic_de_comp *comp, u32 layer_id)
{
	if (comp->config->cap[layer_id].layer_type == AICFB_LAYER_TYPE_UI)
		return true;
	else
		return false;
}

static inline bool is_valid_layer_and_rect_id(struct aic_de_comp *comp,
					      u32 layer_id, u32 rect_id)
{
	u32 flags;

	if (!is_valid_layer_id(comp, layer_id))
		return false;

	flags = comp->config->cap[layer_id].cap_flags;

	if (flags & AICFB_CAP_4_RECT_WIN_FLAG) {
		if (layer_id != 0 && rect_id >= MAX_RECT_NUM)
			return false;
	}
	return true;
}

static inline bool is_support_color_key(struct aic_de_comp *comp,
					u32 layer_id)
{
	u32 flags;

	if (!is_valid_layer_id(comp, layer_id))
		return false;

	flags = comp->config->cap[layer_id].cap_flags;

	if (flags & AICFB_CAP_CK_FLAG)
		return true;
	else
		return false;
}

static inline bool is_support_alpha_blending(struct aic_de_comp *comp,
					     u32 layer_id)
{
	u32 flags;

	if (!is_valid_layer_id(comp, layer_id))
		return false;

	flags = comp->config->cap[layer_id].cap_flags;

	if (flags & AICFB_CAP_ALPHA_FLAG)
		return true;
	else
		return false;
}

static int aic_de_set_mode(struct aic_panel *panel, struct videomode *vm)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();
	int disp_dither = panel->disp_dither;

	memcpy(&comp->vm, vm, sizeof(struct videomode));
	comp->vm_flag = true;
	comp->panel = panel;

	switch (disp_dither) {
	case DITHER_RGB565:
		comp->dither.red_bitdepth = 5;
		comp->dither.gleen_bitdepth = 6;
		comp->dither.blue_bitdepth = 5;
		comp->dither.enable = 1;
		break;
	case DITHER_RGB666:
		comp->dither.red_bitdepth = 6;
		comp->dither.gleen_bitdepth = 6;
		comp->dither.blue_bitdepth = 6;
		comp->dither.enable = 1;
		break;
	default:
		memset(&comp->dither, 0, sizeof(struct aic_de_dither));
		break;
	}

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_clk_enable(void)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();
	int ret = 0;

	ret = clk_set_rate(comp->mclk, comp->mclk_rate);
	if (ret)
		dev_err(comp->dev, "Failed to set CLK_DE %ld\n",
			comp->mclk_rate);

	ret = reset_control_deassert(comp->reset);
	if (ret) {
		dev_err(comp->dev, "%s() - Couldn't deassert\n", __func__);
		aic_de_release_drvdata();
		return ret;
	}

	ret = clk_prepare_enable(comp->mclk);
	if (ret)
		dev_err(comp->dev, "%s() - Couldn't enable mclk\n", __func__);

	aic_de_release_drvdata();
	return ret;
}

static int aic_de_clk_disable(void)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	clk_disable_unprepare(comp->mclk);
	reset_control_assert(comp->reset);

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_timing_enable(u32 flags)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();
	u32 active_w = comp->vm.hactive;
	u32 active_h = comp->vm.vactive;
	u32 hfp = comp->vm.hfront_porch;
	u32 hbp = comp->vm.hback_porch;
	u32 vfp = comp->vm.vfront_porch;
	u32 vbp = comp->vm.vback_porch;
	u32 hsync = comp->vm.hsync_len;
	u32 vsync = comp->vm.vsync_len;
	bool h_pol = !!(comp->vm.flags & DISPLAY_FLAGS_HSYNC_HIGH);
	bool v_pol = !!(comp->vm.flags & DISPLAY_FLAGS_VSYNC_HIGH);
	struct aic_tearing_effect *te = &comp->panel->te;
	int ret;

	if (!comp->vm_flag) {
		dev_err(comp->dev, "%s() - videomode isn't init\n", __func__);
		aic_de_release_drvdata();
		return -EINVAL;
	}

	ret = clk_prepare_enable(comp->pclk);
	if (ret) {
		dev_err(comp->dev, "%s() - Couldn't enable pclk\n", __func__);
		aic_de_release_drvdata();
		return ret;
	}

	if (flags)
		de_config_timing(comp->regs, active_w, active_h, hfp, hbp,
				 vfp, vbp, hsync, vsync, h_pol, v_pol);

	/* set default config */
	de_set_qos(comp->regs);
	de_set_blending_size(comp->regs, active_w, active_h);
	de_set_ui_layer_size(comp->regs, active_w, active_h, 0, 0);
	de_scaler0_active_handle(comp->regs, 0);

	if (!comp->init_flag) {
		comp->alpha[1].layer_id = AICFB_LAYER_TYPE_UI;
		comp->alpha[1].enable = 1;
		comp->alpha[1].mode = AICFB_PIXEL_ALPHA_MODE;
		comp->alpha[1].value = 0xff;
		comp->init_flag = true;
	}

	de_config_prefetch_line_set(comp->regs, 2);
	de_soft_reset_ctrl(comp->regs, 1);

	if (te->mode)
		de_config_tearing_effect(comp->regs,
				te->mode, te->pulse_width);

	if (comp->dither.enable)
		de_set_dither(comp->regs,
			      comp->dither.red_bitdepth,
			      comp->dither.gleen_bitdepth,
			      comp->dither.blue_bitdepth,
			      comp->dither.enable);

	de_ui_alpha_blending_enable(comp->regs, comp->alpha[1].value,
				    comp->alpha[1].mode,
				    comp->alpha[1].enable);

	de_timing_enable_interrupt(comp->regs, true, TIMING_INIT_V_BLANK_FLAG);
	de_timing_enable_interrupt(comp->regs, true, TIMING_INIT_UNDERFLOW_FLAG);

	de_timing_enable(comp->regs, 1);

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_timing_disable(void)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	de_timing_enable(comp->regs, 0);
	clk_disable_unprepare(comp->pclk);
	aic_de_release_drvdata();
	return 0;
}

static int aic_de_get_layer_num(struct aicfb_layer_num *num)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	memcpy(num, comp->config->layer_num, sizeof(struct aicfb_layer_num));
	aic_de_release_drvdata();
	return 0;
}

static int aic_de_get_layer_cap(struct aicfb_layer_capability *cap)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	if (is_valid_layer_id(comp, cap->layer_id) == false) {
		dev_err(comp->dev, "Invalid layer id %d\n", cap->layer_id);
		aic_de_release_drvdata();
		return -EINVAL;
	}
	memcpy(cap, &comp->config->cap[cap->layer_id],
		sizeof(struct aicfb_layer_capability));

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_get_layer_config(struct aicfb_layer_data *layer_data)
{
	u32 index;
	struct aic_de_comp *comp = aic_de_request_drvdata();

	if (is_valid_layer_and_rect_id(comp, layer_data->layer_id,
				       layer_data->rect_id) == false) {
		dev_err(comp->dev, "invalid layer_id %d or rect_id %d\n",
			layer_data->layer_id, layer_data->rect_id);
		aic_de_release_drvdata();
		return -EINVAL;
	}

	index = (layer_data->layer_id << RECT_NUM_SHIFT) + layer_data->rect_id;
	comp->layers[index].layer_id = layer_data->layer_id;
	comp->layers[index].rect_id = layer_data->rect_id;
	memcpy(layer_data, &comp->layers[index],
		sizeof(struct aicfb_layer_data));

	aic_de_release_drvdata();
	return 0;
}

static bool is_valid_video_size(struct aic_de_comp *comp,
				struct aicfb_layer_data *layer_data)
{
	u32 src_width;
	u32 src_height;
	u32 x_offset;
	u32 y_offset;
	u32 active_w;
	u32 active_h;

	if (!comp->vm_flag) {
		dev_err(comp->dev, "videomode isn't init\n");
		return false;
	}

	src_width = layer_data->buf.size.width;
	src_height = layer_data->buf.size.height;
	x_offset = layer_data->pos.x;
	y_offset = layer_data->pos.y;
	active_w = comp->vm.hactive;
	active_h = comp->vm.vactive;

	if (x_offset >= active_w || y_offset >= active_h) {
		dev_err(comp->dev, "video layer x or y offset is invalid\n");
		return false;
	}

	if (layer_data->buf.crop_en) {
		u32 crop_x = layer_data->buf.crop.x;
		u32 crop_y = layer_data->buf.crop.y;

		if (crop_x >= src_width || crop_y >= src_height) {
			dev_err(comp->dev, "video layer crop is invalid\n");
			return false;
		}

		if (crop_x + layer_data->buf.crop.width > src_width)
			layer_data->buf.crop.width = src_width - crop_x;

		if (crop_y + layer_data->buf.crop.height > src_height)
			layer_data->buf.crop.height = src_height - crop_y;
	}

	if (x_offset + layer_data->scale_size.width > active_w)
		layer_data->scale_size.width = active_w - x_offset;

	if (y_offset + layer_data->scale_size.height > active_h)
		layer_data->scale_size.height = active_h - y_offset;

	return true;
}

static bool is_ui_rect_win_overlap(struct aic_de_comp *comp,
				   u32 layer_id, u32 rect_id,
				   u32 x, u32 y, u32 w, u32 h)
{
	int i;
	u32 index;
	u32 cur_x, cur_y;
	u32 cur_w, cur_h;

	index = (layer_id << RECT_NUM_SHIFT);

	for (i = 0; i < MAX_RECT_NUM; i++) {
		if (rect_id == i) {
			index++;
			continue;
		}

		if (comp->layers[index].enable) {
			if (comp->layers[index].buf.crop_en) {
				cur_w = comp->layers[index].buf.crop.width;
				cur_h = comp->layers[index].buf.crop.height;
			} else {
				cur_w = comp->layers[index].buf.size.width;
				cur_h = comp->layers[index].buf.size.height;
			}
			cur_x = comp->layers[index].pos.x;
			cur_y = comp->layers[index].pos.y;

			if ((min(y + h, cur_y + cur_h) > max(y, cur_y)) &&
			    (min(x + w, cur_x + cur_w) > max(x, cur_x))) {
				return true;
			}
		}
		index++;
	}
	return false;
}

static bool is_valid_ui_rect_size(struct aic_de_comp *comp,
				  struct aicfb_layer_data *layer_data)
{
	u32 src_width;
	u32 src_height;
	u32 x_offset;
	u32 y_offset;
	u32 active_w;
	u32 active_h;

	u32 w;
	u32 h;

	if (!comp->vm_flag) {
		dev_err(comp->dev, "videomode isn't init\n");
		return false;
	}

	src_width = layer_data->buf.size.width;
	src_height = layer_data->buf.size.height;
	x_offset = layer_data->pos.x;
	y_offset = layer_data->pos.y;
	active_w = comp->vm.hactive;
	active_h = comp->vm.vactive;

	if (x_offset >= active_w || y_offset >= active_h) {
		dev_err(comp->dev, "ui layer x or y offset is invalid\n");
		return false;
	}

	if (layer_data->buf.crop_en) {
		u32 crop_x = layer_data->buf.crop.x;
		u32 crop_y = layer_data->buf.crop.y;

		if (crop_x >= src_width || crop_y >= src_height) {
			dev_err(comp->dev, "ui layer crop is invalid\n");
			return false;
		}

		if ((crop_x + layer_data->buf.crop.width) > src_width)
			layer_data->buf.crop.width = src_width - crop_x;

		if ((crop_y + layer_data->buf.crop.height) > src_height)
			layer_data->buf.crop.height = src_height - crop_y;

		if ((x_offset + layer_data->buf.crop.width) > active_w)
			layer_data->buf.crop.width = active_w - x_offset;

		if ((y_offset + layer_data->buf.crop.height) > active_h)
			layer_data->buf.crop.height = active_h - y_offset;

		w = layer_data->buf.crop.width;
		h = layer_data->buf.crop.height;
	} else {
		if ((x_offset + src_width) > active_w)
			layer_data->buf.size.width = active_w - x_offset;

		if ((y_offset + src_height) > active_h)
			layer_data->buf.size.height = active_h - y_offset;

		w = layer_data->buf.size.width;
		h = layer_data->buf.size.height;
	}

	/* check overlap  */
	if (is_ui_rect_win_overlap(comp, layer_data->layer_id,
				   layer_data->rect_id,
				   x_offset, y_offset,	w, h)) {
		dev_err(comp->dev, "ui rect is overlap\n");
		return false;
	}
	return true;
}

static inline bool is_all_rect_win_disabled(struct aic_de_comp *comp,
					    u32 layer_id)
{
	int i;
	u32 index;

	index = (layer_id << RECT_NUM_SHIFT);

	for (i = 0; i < MAX_RECT_NUM; i++) {
		if (comp->layers[index].enable)
			return false;

		index++;
	}
	return true;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static struct aic_de_dmabuf *aic_de_fd2index(struct aic_de_comp *comp, int fd)
{
	struct aic_de_dmabuf *pos = NULL;

	mutex_lock(&comp->mutex);
	list_for_each_entry(pos, &comp->dma_buf, list) {
		if (pos->fds.fd == fd) {
			mutex_unlock(&comp->mutex);
			return pos;
		}
	}
	mutex_unlock(&comp->mutex);

	dev_err(comp->dev, "failed to find out the given fd %d\n", fd);
	return NULL;
}

static int config_video_dmabuf(struct aic_de_comp *comp,
		struct mpp_buf *buf, unsigned int *phy_addr)
{
	int i;
	struct aic_de_dmabuf *dbuf = NULL;

	for (i = 0; i < AICFB_PLANE_NUM; i++) {
		if (buf->fd[i] <= 0)
			continue;

		dbuf = aic_de_fd2index(comp, buf->fd[i]);
		if (dbuf == NULL)
			return -1;

		phy_addr[i] = dbuf->phy_addr;
	}
	return 0;
}
#endif

static int config_ui_layer_rect(struct aic_de_comp *comp,
				struct aicfb_layer_data *layer_data)
{
	enum mpp_pixel_format format = layer_data->buf.format;
	u32 in_w = (u32)layer_data->buf.size.width;
	u32 in_h = (u32)layer_data->buf.size.height;

	u32 stride0 = layer_data->buf.stride[0];
	u32 addr0 = layer_data->buf.phy_addr[0];
	u32 x_offset = layer_data->pos.x;
	u32 y_offset = layer_data->pos.y;
	u32 crop_en = layer_data->buf.crop_en;
	u32 crop_x = layer_data->buf.crop.x;
	u32 crop_y = layer_data->buf.crop.y;
	u32 crop_w = layer_data->buf.crop.width;
	u32 crop_h = layer_data->buf.crop.height;
	u32 flags = layer_data->buf.flags;

	if (MPP_BUF_PAN_DISPLAY_DMABUF_GET(flags)) {
		unsigned int phy_addr[3] = {0};

		if (config_video_dmabuf(comp, &layer_data->buf, phy_addr) < 0)
			return -EINVAL;

		addr0 = phy_addr[0];
		layer_data->buf.phy_addr[0] = addr0;
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
		if (crop_en) {
			addr0 += crop_x * 4 + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
		break;
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
		if (crop_en) {
			addr0 += crop_x * 3 + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
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
		if (crop_en) {
			addr0 += crop_x * 2 + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
		break;
	default:
		dev_err(comp->dev, "invalid ui layer format: %d\n",
			format);

		return -EINVAL;
	}

	if (is_all_rect_win_disabled(comp, layer_data->layer_id)) {
		de_set_ui_layer_format(comp->regs, format);
		de_ui_layer_enable(comp->regs, 1);
	}

	de_ui_layer_set_rect(comp->regs, in_w, in_h, x_offset, y_offset,
			     stride0, addr0, layer_data->rect_id);
	de_ui_layer_rect_enable(comp->regs, layer_data->rect_id, 1);

	return 0;
}

static int de_update_csc(struct aic_de_comp *comp,
			 struct aicfb_disp_prop *disp_prop,
			 int color_space)
{
	if (need_use_hsbc(comp, disp_prop)) {
		int bright = (int)disp_prop->bright;
		int contrast = (int)disp_prop->contrast;
		int saturation = (int)disp_prop->saturation;
		int hue = (int)disp_prop->hue;

		de_set_hsbc_with_csc_coefs(comp->regs, color_space,
					   bright, contrast,
					   saturation, hue);
	} else {
		de_set_csc0_coefs(comp->regs, color_space);
	}

	return 0;
}

static int config_video_layer(struct aic_de_comp *comp,
			      struct aicfb_layer_data *layer_data)
{
	enum mpp_pixel_format format = layer_data->buf.format;
	u32 in_w = (u32)layer_data->buf.size.width;
	u32 in_h = (u32)layer_data->buf.size.height;
	u32 in_w_ch1;
	u32 in_h_ch1;
	u32 stride0 = layer_data->buf.stride[0];
	u32 stride1 = layer_data->buf.stride[1];
	u32 addr0, addr1, addr2;
	u32 x_offset = layer_data->pos.x;
	u32 y_offset = layer_data->pos.y;
	u32 crop_en = layer_data->buf.crop_en;
	u32 crop_x = layer_data->buf.crop.x;
	u32 crop_y = layer_data->buf.crop.y;
	u32 crop_w = layer_data->buf.crop.width;
	u32 crop_h = layer_data->buf.crop.height;
	u32 tile_p0_x_offset = 0;
	u32 tile_p0_y_offset = 0;
	u32 tile_p1_x_offset = 0;
	u32 tile_p1_y_offset = 0;
	u32 channel_num = 1;
	u32 scaler_en = 0;
	u32 scaler_w = layer_data->scale_size.width;
	u32 scaler_h = layer_data->scale_size.height;
	int color_space = MPP_BUF_COLOR_SPACE_GET(layer_data->buf.flags);

	if (!scaler_w) {
		scaler_w = in_w;
		layer_data->scale_size.width = in_w;
	}

	if (!scaler_h) {
		scaler_h = in_h;
		layer_data->scale_size.height = in_h;
	}

	switch (layer_data->buf.buf_type) {
#ifdef CONFIG_DMA_SHARED_BUFFER
	case MPP_DMA_BUF_FD:
	{
		unsigned int phy_addr[3] = {0};

		if (config_video_dmabuf(comp, &layer_data->buf, phy_addr) < 0)
			return -EINVAL;

		addr0 = phy_addr[0];
		addr1 = phy_addr[1];
		addr2 = phy_addr[2];
		break;
	}
#endif
	case MPP_PHY_ADDR:
		addr0 = layer_data->buf.phy_addr[0];
		addr1 = layer_data->buf.phy_addr[1];
		addr2 = layer_data->buf.phy_addr[2];
		break;
	default:
		dev_err(comp->dev, "invalid buf type: %d\n",
			layer_data->buf.buf_type);

		return -EINVAL;
	};

	switch (format) {
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_XRGB_8888:
	case MPP_FMT_XBGR_8888:
	case MPP_FMT_RGBX_8888:
	case MPP_FMT_BGRX_8888:
		if (crop_en) {
			addr0 += crop_x * 4 + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
		break;
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_888:
		if (crop_en) {
			addr0 += crop_x * 3 + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
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
		if (crop_en) {
			addr0 += crop_x * 2 + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
		break;
	case MPP_FMT_YUV420P:
		channel_num = 2;
		scaler_en = 1;
		in_w = ALIGN_EVEN(in_w);
		in_h = ALIGN_EVEN(in_h);
		crop_x = ALIGN_EVEN(crop_x);
		crop_y = ALIGN_EVEN(crop_y);
		crop_w = ALIGN_EVEN(crop_w);
		crop_h = ALIGN_EVEN(crop_h);

		if (crop_en) {
			u32 ch1_offset;

			addr0 += crop_x + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;

			ch1_offset = (crop_x >> 1)
				     + (crop_y >> 1) * stride1;

			addr1 += ch1_offset;
			addr2 += ch1_offset;
		}

		in_w_ch1 = in_w >> 1;
		in_h_ch1 = in_h >> 1;
		break;
	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
		channel_num = 2;
		scaler_en = 1;
		in_w = ALIGN_EVEN(in_w);
		in_h = ALIGN_EVEN(in_h);
		crop_x = ALIGN_EVEN(crop_x);
		crop_y = ALIGN_EVEN(crop_y);
		crop_w = ALIGN_EVEN(crop_w);
		crop_h = ALIGN_EVEN(crop_h);

		if (crop_en) {
			addr0 += crop_x + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;

			addr1 += crop_x + (crop_y >> 1) * stride1;
		}

		in_w_ch1 = in_w >> 1;
		in_h_ch1 = in_h >> 1;

		break;
	case MPP_FMT_YUV420_64x32_TILE:
		channel_num = 2;
		scaler_en = 1;
		in_w = ALIGN_EVEN(in_w);
		in_h = ALIGN_EVEN(in_h);
		crop_x = ALIGN_EVEN(crop_x);
		crop_y = ALIGN_EVEN(crop_y);
		crop_w = ALIGN_EVEN(crop_w);
		crop_h = ALIGN_EVEN(crop_h);

		if (crop_en) {
			u32 tile_p0_x, tile_p0_y;
			u32 tile_p1_x, tile_p1_y;
			u32 offset_p0, offset_p1;

			tile_p0_x = crop_x >> 6;
			tile_p0_x_offset = crop_x & 63;
			tile_p0_y = crop_y >> 5;
			tile_p0_y_offset = crop_y & 31;

			tile_p1_x = crop_x >> 6;
			tile_p1_x_offset = crop_x & 63;
			tile_p1_y = (crop_y >> 1) >> 5;
			tile_p1_y_offset = (crop_y >> 1) & 31;

			offset_p0 = ALIGN_64B(stride0) * 32 * tile_p0_y
				    + 64 * 32 * tile_p0_x;

			offset_p1 = ALIGN_64B(stride1) * 32 * tile_p1_y
				    + 64 * 32 * tile_p1_x;

			addr0 += offset_p0;
			addr1 += offset_p1;

			in_w = crop_w;
			in_h = crop_h;
		}

		in_w_ch1 = in_w >> 1;
		in_h_ch1 = in_h >> 1;
		break;
	case MPP_FMT_YUV420_128x16_TILE:
		channel_num = 2;
		scaler_en = 1;
		in_w = ALIGN_EVEN(in_w);
		in_h = ALIGN_EVEN(in_h);
		crop_x = ALIGN_EVEN(crop_x);
		crop_y = ALIGN_EVEN(crop_y);
		crop_w = ALIGN_EVEN(crop_w);
		crop_h = ALIGN_EVEN(crop_h);

		if (crop_en) {
			u32 tile_p0_x, tile_p0_y;
			u32 tile_p1_x, tile_p1_y;
			u32 offset_p0, offset_p1;

			tile_p0_x = crop_x >> 7;
			tile_p0_x_offset = crop_x & 127;

			tile_p0_y = crop_y >> 4;
			tile_p0_y_offset = crop_y & 15;

			tile_p1_x = crop_x >> 7;
			tile_p1_x_offset = crop_x & 127;

			tile_p1_y = (crop_y >> 1) >> 4;
			tile_p1_y_offset = (crop_y >> 1) & 15;

			offset_p0 = ALIGN_128B(stride0) * 16 * tile_p0_y
				    + 128 * 16 * tile_p0_x;

			offset_p1 = ALIGN_128B(stride1) * 16 * tile_p1_y
				    + 128 * 16 * tile_p1_x;

			addr0 += offset_p0;
			addr1 += offset_p1;

			in_w = crop_w;
			in_h = crop_h;
		}

		in_w_ch1 = in_w >> 1;
		in_h_ch1 = in_h >> 1;
		break;
	case MPP_FMT_YUV400:
		channel_num = 1;
		scaler_en = 1;

		if (crop_en) {
			addr0 += crop_x + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}
		break;
	case MPP_FMT_YUV422P:
		channel_num = 2;
		scaler_en = 1;

		in_w = ALIGN_EVEN(in_w);
		crop_x = ALIGN_EVEN(crop_x);
		crop_w = ALIGN_EVEN(crop_w);

		if (crop_en) {
			u32 ch1_offset;

			addr0 += crop_x + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;

			ch1_offset = (crop_x >> 1) + crop_y * stride1;
			addr1 += ch1_offset;
			addr2 += ch1_offset;
		}

		in_w_ch1 = in_w >> 1;
		in_h_ch1 = in_h;
		break;
	case MPP_FMT_NV16:
	case MPP_FMT_NV61:
		channel_num = 2;
		scaler_en = 1;
		in_w = ALIGN_EVEN(in_w);
		crop_x = ALIGN_EVEN(crop_x);
		crop_w = ALIGN_EVEN(crop_w);

		if (crop_en) {
			u32 ch1_offset;

			addr0 += crop_x + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;

			ch1_offset = crop_x + crop_y * stride1;
			addr1 += ch1_offset;
		}

		in_w_ch1 = in_w >> 1;
		in_h_ch1 = in_h;
		break;
	case MPP_FMT_YUYV:
	case MPP_FMT_YVYU:
	case MPP_FMT_UYVY:
	case MPP_FMT_VYUY:
		channel_num = 2;
		scaler_en = 1;
		in_w = ALIGN_EVEN(in_w);
		crop_x = ALIGN_EVEN(crop_x);
		crop_w = ALIGN_EVEN(crop_w);

		if (crop_en) {
			addr0 += (crop_x << 1) + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
		}

		in_w_ch1 = in_w >> 1;
		in_h_ch1 = in_h;
		break;
	case MPP_FMT_YUV422_64x32_TILE:
		channel_num = 2;
		scaler_en = 1;
		in_w = ALIGN_EVEN(in_w);
		crop_x = ALIGN_EVEN(crop_x);
		crop_w = ALIGN_EVEN(crop_w);

		if (crop_en) {
			u32 tile_p0_x, tile_p0_y;
			u32 tile_p1_x, tile_p1_y;
			u32 offset_p0, offset_p1;

			tile_p0_x = crop_x >> 6;
			tile_p0_x_offset = crop_x & 63;
			tile_p0_y = crop_y >> 5;
			tile_p0_y_offset = crop_y & 31;

			tile_p1_x = crop_x >> 6;
			tile_p1_x_offset = crop_x & 63;
			tile_p1_y = crop_y >> 5;
			tile_p1_y_offset = crop_y & 31;

			offset_p0 = ALIGN_64B(stride0) * 32 * tile_p0_y
				    + 64 * 32 * tile_p0_x;

			offset_p1 = ALIGN_64B(stride1) * 32 * tile_p1_y
				    + 64 * 32 * tile_p1_x;

			addr0 += offset_p0;
			addr1 += offset_p1;
			in_w = crop_w;
			in_h = crop_h;
		}

		in_w_ch1 = in_w >> 1;
		in_h_ch1 = in_h;
		break;
	case MPP_FMT_YUV422_128x16_TILE:
		channel_num = 2;
		scaler_en = 1;
		in_w = ALIGN_EVEN(in_w);
		crop_x = ALIGN_EVEN(crop_x);
		crop_w = ALIGN_EVEN(crop_w);

		if (crop_en) {
			u32 tile_p0_x, tile_p0_y;
			u32 tile_p1_x, tile_p1_y;
			u32 offset_p0, offset_p1;

			tile_p0_x = crop_x >> 7;
			tile_p0_x_offset = crop_x & 127;

			tile_p0_y = crop_y >> 4;
			tile_p0_y_offset = crop_y & 15;

			tile_p1_x = crop_x >> 7;
			tile_p1_x_offset = crop_x & 127;

			tile_p1_y = crop_y >> 4;
			tile_p1_y_offset = crop_y & 15;

			offset_p0 = ALIGN_128B(stride0) * 16 * tile_p0_y
				    + 128 * 16 * tile_p0_x;

			offset_p1 = ALIGN_128B(stride1) * 16 * tile_p1_y
				    + 128 * 16 * tile_p1_x;

			addr0 += offset_p0;
			addr1 += offset_p1;
			in_w = crop_w;
			in_h = crop_h;
		}

		in_w_ch1 = in_w >> 1;
		in_h_ch1 = in_h;
		break;
	case MPP_FMT_YUV444P:
		channel_num = 2;
		scaler_en = 0;

		if (crop_en) {
			u32 ch1_offset;

			addr0 += crop_x + crop_y * stride0;
			in_w = crop_w;
			in_h = crop_h;
			ch1_offset = crop_x + crop_y * stride1;
			addr1 += ch1_offset;
			addr2 += ch1_offset;
		}

		break;
	default:
		dev_err(comp->dev, "invalid video layer format: %d\n",
			format);

		return -EINVAL;
	}

	de_set_video_layer_info(comp->regs, in_w, in_h, format,
				stride0, stride1, addr0, addr1, addr2,
				x_offset, y_offset);

	de_set_video_layer_tile_offset(comp->regs,
				       tile_p0_x_offset, tile_p0_y_offset,
				       tile_p1_x_offset, tile_p1_y_offset);

	if (scaler_en) {
		if (need_update_csc(comp, color_space)) {
			struct aicfb_disp_prop *disp_prop = &comp->disp_prop;

			de_update_csc(comp, disp_prop, color_space);
		}

		de_set_scaler0_channel(comp->regs, in_w, in_h,
				       scaler_w, scaler_h, 0);

		if (channel_num == 2) {
			de_set_scaler0_channel(comp->regs,
					       in_w_ch1, in_h_ch1,
					       scaler_w, scaler_h, 1);

			de_check_scaler0_active(comp, in_w_ch1, in_h_ch1,
						scaler_w, scaler_h);
		}

		de_scaler0_enable(comp->regs, 1);
	} else {
		de_scaler0_enable(comp->regs, 0);
	}

	de_video_layer_enable(comp->regs, 1);

	return 0;
}

static inline int ui_rect_disable(struct aic_de_comp *comp,
				  u32 layer_id, u32 rect_id)
{
	de_ui_layer_rect_enable(comp->regs, rect_id, 0);
	if (is_all_rect_win_disabled(comp, layer_id))
		de_ui_layer_enable(comp->regs, 0);
	return 0;
}

static int update_one_layer_config(struct aic_de_comp *comp,
				   struct aicfb_layer_data *layer_data)
{
	u32 index;
	int ret;

	if (!is_valid_layer_and_rect_id(comp, layer_data->layer_id,
				       layer_data->rect_id)) {
		dev_err(comp->dev,
			"%s() - layer_id %d or rect_id %d is invalid\n",
			__func__, layer_data->layer_id, layer_data->rect_id);
		return -EINVAL;
	}

	if (layer_data->enable == 0) {
		index = (layer_data->layer_id << RECT_NUM_SHIFT)
			+ layer_data->rect_id;
		comp->layers[index].enable = 0;

		if (is_ui_layer(comp, layer_data->layer_id)) {
			ui_rect_disable(comp, layer_data->layer_id,
					layer_data->rect_id);
		} else {
			de_video_layer_enable(comp->regs, 0);
		}
		return 0;
	}

	if (is_ui_layer(comp, layer_data->layer_id)) {
		index = (layer_data->layer_id << RECT_NUM_SHIFT)
			+ layer_data->rect_id;

		if (!is_valid_ui_rect_size(comp, layer_data))
			return -EINVAL;

		ret = config_ui_layer_rect(comp, layer_data);
		if (ret != 0)
			return -EINVAL;
		else
			memcpy(&comp->layers[index], layer_data,
			       sizeof(struct aicfb_layer_data));
	} else {
		index = layer_data->layer_id << RECT_NUM_SHIFT;

		if (!is_valid_video_size(comp, layer_data)) {
			comp->layers[index].enable = 0;
			de_video_layer_enable(comp->regs, 0);
			return -EINVAL;
		}

		ret = config_video_layer(comp, layer_data);
		if (ret != 0) {
			comp->layers[index].enable = 0;
			de_video_layer_enable(comp->regs, 0);
		} else {
			memcpy(&comp->layers[index], layer_data,
			       sizeof(struct aicfb_layer_data));
		}
		return ret;
	}
	return 0;
}

static int aic_de_update_layer_config(struct aicfb_layer_data *layer_data)
{
	int ret;
	struct aic_de_comp *comp = aic_de_request_drvdata();

	de_config_update_enable(comp->regs, 0);
	ret = update_one_layer_config(comp, layer_data);
	de_config_update_enable(comp->regs, 1);

	aic_de_release_drvdata();
	return ret;
}

static int aic_de_set_display_prop(struct aicfb_disp_prop *disp_prop)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	de_config_update_enable(comp->regs, 0);

	if (need_update_disp_prop(comp, disp_prop)) {
		/* get color space from video layer config */
		struct aicfb_layer_data *layer_data = &comp->layers[0];
		int color_space = MPP_BUF_COLOR_SPACE_GET(layer_data->buf.flags);

		de_update_csc(comp, disp_prop, color_space);
	}

	de_config_update_enable(comp->regs, 1);

	memcpy(&comp->disp_prop, disp_prop, sizeof(struct aicfb_disp_prop));

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_get_display_prop(struct aicfb_disp_prop *disp_prop)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	memcpy(disp_prop, &comp->disp_prop, sizeof(struct aicfb_disp_prop));

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_update_layer_config_list(struct aicfb_config_lists *list)
{
	int ret, i;
	struct aic_de_comp *comp = aic_de_request_drvdata();

	de_config_update_enable(comp->regs, 0);

	for (i = 0; i < list->num; i++) {
		if (is_ui_layer(comp, list->layers[i].layer_id)) {
			int index = (list->layers[i].layer_id << RECT_NUM_SHIFT)
					+ list->layers[i].rect_id;
			comp->layers[index].enable = 0;
		}
	}

	for (i = 0; i < list->num; i++) {
		ret = update_one_layer_config(comp, &list->layers[i]);
		if (ret)
			return ret;
	}
	de_config_update_enable(comp->regs, 1);

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_shadow_reg_ctrl(int enable)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	de_config_update_enable(comp->regs, (u32)enable);
	aic_de_release_drvdata();
	return 0;
}

static int aic_de_get_alpha_config(struct aicfb_alpha_config *alpha)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	if (is_support_alpha_blending(comp, alpha->layer_id) == false) {
		dev_err(comp->dev, "layer %d doesn't support alpha blending\n",
			alpha->layer_id);
		aic_de_release_drvdata();
		return -EINVAL;
	}

	comp->alpha[alpha->layer_id].layer_id = alpha->layer_id;
	memcpy(alpha, &comp->alpha[alpha->layer_id],
		sizeof(struct aicfb_alpha_config));

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_update_alpha_config(struct aicfb_alpha_config *alpha)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	if (is_support_alpha_blending(comp, alpha->layer_id) == false) {
		dev_err(comp->dev, "layer %d doesn't support alpha blending\n",
				alpha->layer_id);
		aic_de_release_drvdata();
		return -EINVAL;
	}

	de_config_update_enable(comp->regs, 0);
	de_ui_alpha_blending_enable(comp->regs,
		alpha->value, alpha->mode, alpha->enable);
	de_config_update_enable(comp->regs, 1);
	memcpy(&comp->alpha[alpha->layer_id],
		alpha, sizeof(struct aicfb_alpha_config));

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_get_ck_config(struct aicfb_ck_config *ck)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	if (is_support_color_key(comp, ck->layer_id) == false) {
		dev_err(comp->dev,
			"Layer %d doesn't support color key blending\n",
			ck->layer_id);
		aic_de_release_drvdata();
		return -EINVAL;
	}
	comp->ck[ck->layer_id].layer_id = ck->layer_id;
	memcpy(ck, &comp->ck[ck->layer_id], sizeof(struct aicfb_ck_config));

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_update_ck_config(struct aicfb_ck_config *ck)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	if (is_support_color_key(comp, ck->layer_id) == false) {
		dev_err(comp->dev,
			"Layer %d doesn't support color key blending\n",
			ck->layer_id);
		aic_de_release_drvdata();
		return -EINVAL;
	}
	if (ck->enable > 1) {
		dev_err(comp->dev, "Invalid ck enable: %d\n", ck->enable);
		aic_de_release_drvdata();
		return -EINVAL;
	}

	de_config_update_enable(comp->regs, 0);
	de_ui_layer_color_key_enable(comp->regs, ck->value, ck->enable);
	de_config_update_enable(comp->regs, 1);
	memcpy(&comp->ck[ck->layer_id], ck, sizeof(struct aicfb_ck_config));

	aic_de_release_drvdata();
	return 0;
}

static int aic_de_wait_for_vsync(void)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&comp->slock, flags);
	comp->vsync_flag = 0;
	spin_unlock_irqrestore(&comp->slock, flags);

	ret = wait_event_interruptible_timeout(comp->vsync_wait,
					       comp->vsync_flag != 0,
					       msecs_to_jiffies(100));

	if (ret < 0)
		return ret;
	if (ret == 0)
		return -ETIMEDOUT;

	return 0;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static int aic_de_add_dmabuf(struct dma_buf_info *fds)
{
	int ret = 0;
	struct aic_de_comp *comp = aic_de_request_drvdata();
	struct aic_de_dmabuf *dmabuf = NULL;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	struct dma_buf *buf;

	dmabuf = kmalloc(sizeof(*dmabuf), GFP_KERNEL);
	if (!dmabuf) {
		dev_err(comp->dev, "kmalloc dmabuf failed!\n");
		return -ENOMEM;
	}

	memset(dmabuf, 0, sizeof(struct aic_de_dmabuf));
	memcpy(&dmabuf->fds, fds, sizeof(*fds));

	buf = dma_buf_get(fds->fd);
	if (IS_ERR(buf)) {
		dev_err(comp->dev, "dma_buf_get(%d) failed\n",
			fds->fd);
		ret = PTR_ERR(buf);
		goto end;
	}

	attach = dma_buf_attach(buf, comp->dev);
	if (IS_ERR(attach)) {
		dev_err(comp->dev, "dma_buf_attach(%d) failed\n",
			fds->fd);
		dma_buf_put(buf);
		ret = PTR_ERR(attach);
		goto end;
	}

	sgt = dma_buf_map_attachment(attach, DMA_TO_DEVICE);
	if (IS_ERR(sgt)) {
		dev_err(comp->dev,
			"dma_buf_map_attachment(%d) failed\n",
			fds->fd);
		dma_buf_detach(buf, attach);
		dma_buf_put(buf);
		ret = PTR_ERR(sgt);
		goto end;
	}

	dmabuf->buf = buf;
	dmabuf->attach = attach;
	dmabuf->sgt = sgt;
	/* The scatterlist in sgt must be physically consecutive.
	 * So we consider the first dma_address as the buffer begin. */
	dmabuf->phy_addr = sg_dma_address(sgt->sgl);
	dmabuf->pid = current->tgid;
	fds->phy_addr = dmabuf->phy_addr;

	mutex_lock(&comp->mutex);
	list_add_tail(&dmabuf->list, &comp->dma_buf);
	mutex_unlock(&comp->mutex);

	aic_de_release_drvdata();
	return 0;

end:
	kfree(dmabuf);
	aic_de_release_drvdata();
	return -1;
}

static int aic_de_remove_dmabuf(struct dma_buf_info *fds)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();
	struct aic_de_dmabuf *dmabuf = NULL;

	mutex_lock(&comp->mutex);
	if (!list_empty(&comp->dma_buf)) {
		struct aic_de_dmabuf *pos = NULL, *n = NULL;

		list_for_each_entry_safe(pos, n, &comp->dma_buf, list) {
			if ((pos->fds.fd == fds->fd) && (pos->pid == current->tgid)) {
				dmabuf = pos;
				dma_buf_unmap_attachment(dmabuf->attach,
					dmabuf->sgt, DMA_TO_DEVICE);
				dma_buf_detach(dmabuf->buf, dmabuf->attach);
				dma_buf_put(dmabuf->buf);

				dmabuf->buf = NULL;
				dmabuf->attach = NULL;
				dmabuf->sgt = NULL;
				dmabuf->phy_addr = 0;
				dmabuf->fds.fd = 0;

				list_del_init(&dmabuf->list);
				kfree(dmabuf);
				break;
			}
		}

	}
	aic_de_release_drvdata();
	mutex_unlock(&comp->mutex);
	return 0;
}

/* Called when /dev/fb0 closes. */
static int aic_de_release_dmabuf(void)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();
	struct aic_de_dmabuf *dmabuf = NULL;

	mutex_lock(&comp->mutex);
	if (!list_empty(&comp->dma_buf)) {
		struct aic_de_dmabuf *pos = NULL, *n = NULL;

		list_for_each_entry_safe(pos, n, &comp->dma_buf, list) {
			if (pos->pid == current->tgid) {
				dmabuf = pos;
				dma_buf_unmap_attachment(dmabuf->attach,
					dmabuf->sgt, DMA_TO_DEVICE);
				dma_buf_detach(dmabuf->buf, dmabuf->attach);
				dma_buf_put(dmabuf->buf);
				list_del_init(&dmabuf->list);
				kfree(dmabuf);
			}
		}
	}
	aic_de_release_drvdata();
	mutex_unlock(&comp->mutex);
	return 0;
}
#endif

static int aic_de_color_bar_ctrl(int enable)
{
	struct aic_de_comp *comp = aic_de_request_drvdata();

	de_config_update_enable(comp->regs, 0);
	de_colorbar_ctrl(comp->regs, (u32)enable);
	de_config_update_enable(comp->regs, 1);
	aic_de_release_drvdata();
	return 0;
}

static ulong aic_de_pixclk_rate(void)
{
	ulong rate;
	struct aic_de_comp *comp = aic_de_request_drvdata();

	rate = comp->vm.pixelclock;
	aic_de_release_drvdata();
	return rate;
}

static int aic_de_pixclk_enable(void)
{
	s32 ret = 0;
	struct aic_de_comp *comp = aic_de_request_drvdata();

	ret = clk_set_rate(comp->pclk, comp->vm.pixelclock);
	if (ret)
		dev_err(comp->dev, "Failed to set CLK_PIX %ld\n",
			comp->vm.pixelclock);

	aic_de_release_drvdata();
	return ret;
}

static int aic_de_parse_dt(struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;
	struct aic_de_comp *comp = dev_get_drvdata(dev);

	ret = of_property_read_u32(np, "mclk", (u32 *)&comp->mclk_rate);
	if (ret) {
		dev_err(dev, "Can't parse CLK_DE rate\n");
		return ret;
	}

	return 0;
}

#define UI_RECT(x)	((AICFB_LAYER_TYPE_UI << RECT_NUM_SHIFT) + (x))

static ssize_t display_show(struct device *dev,
				 struct device_attribute *devattr,
				 char *buf)
{
	struct aic_de_comp *comp = dev_get_drvdata(dev);

	sprintf(buf, "Video Layer Enable \t : %u\n"
		"Video Layer Format \t : %d\n"
		"Video Layer Input Size \t : %d x %d\n"
		"Video Layer Stride \t : %d %d\n"
		"Scaler Output Size \t : %d x %d\n"
		"UI Layer Control \t : format: %d, color key: %d, alpha: %d\n"
		"UI Layer Input Size \t : %d x %d\n"
		"UI Layer Color Key \t : %x\n"
		"UI Layer Alpha \t\t : mode: %d, g_alpha: %d\n"
		"UI Rectangle Control \t : %d %d %d %d\n"
		"UI Rectangle Size \t : "
			"(%d, %d) (%d, %d) (%d, %d) (%d, %d)\n"
		"UI Rectangle Offset \t : "
			"(%d, %d) (%d, %d) (%d, %d) (%d, %d)\n"
		"UI Rectangle Stride \t : %d %d %d %d\n"
		"Tearing-effect\t\t : TE mode: %d, TE pulse width: %d\n"
		"Display Dither\t\t : dither_en: %d, "
			"red depth: %d, green depth: %d, blue depth: %d\n"
		"Display Timing \t\t : hactive: %d, vactive: %d "
			"hfp: %d hbp: %d vfp: %d vbp: %d "
			"hsync: %d vsync: %d\n"
		"Display Pixelclock \t : %ld HZ\n",
		comp->layers[AICFB_LAYER_TYPE_VIDEO].enable,
		comp->layers[AICFB_LAYER_TYPE_VIDEO].buf.format,
		comp->layers[AICFB_LAYER_TYPE_VIDEO].buf.size.width,
		comp->layers[AICFB_LAYER_TYPE_VIDEO].buf.size.height,
		comp->layers[AICFB_LAYER_TYPE_VIDEO].buf.stride[0],
		comp->layers[AICFB_LAYER_TYPE_VIDEO].buf.stride[1],
		comp->layers[AICFB_LAYER_TYPE_VIDEO].scale_size.width,
		comp->layers[AICFB_LAYER_TYPE_VIDEO].scale_size.height,
		comp->layers[UI_RECT(0)].buf.format,
		comp->ck[AICFB_LAYER_TYPE_UI].enable,
		comp->alpha[AICFB_LAYER_TYPE_UI].enable,
		comp->vm.hactive,
		comp->vm.vactive,
		comp->ck[AICFB_LAYER_TYPE_UI].value,
		comp->alpha[AICFB_LAYER_TYPE_UI].mode,
		comp->alpha[AICFB_LAYER_TYPE_UI].value,
		comp->layers[UI_RECT(0)].enable,
		comp->layers[UI_RECT(1)].enable,
		comp->layers[UI_RECT(2)].enable,
		comp->layers[UI_RECT(3)].enable,
		comp->layers[UI_RECT(0)].buf.size.width,
		comp->layers[UI_RECT(0)].buf.size.height,
		comp->layers[UI_RECT(1)].buf.size.width,
		comp->layers[UI_RECT(1)].buf.size.height,
		comp->layers[UI_RECT(2)].buf.size.width,
		comp->layers[UI_RECT(2)].buf.size.height,
		comp->layers[UI_RECT(3)].buf.size.width,
		comp->layers[UI_RECT(3)].buf.size.height,
		comp->layers[UI_RECT(0)].pos.x,
		comp->layers[UI_RECT(0)].pos.y,
		comp->layers[UI_RECT(1)].pos.x,
		comp->layers[UI_RECT(1)].pos.y,
		comp->layers[UI_RECT(2)].pos.x,
		comp->layers[UI_RECT(2)].pos.y,
		comp->layers[UI_RECT(3)].pos.x,
		comp->layers[UI_RECT(3)].pos.y,
		comp->layers[UI_RECT(0)].buf.stride[0],
		comp->layers[UI_RECT(1)].buf.stride[0],
		comp->layers[UI_RECT(2)].buf.stride[0],
		comp->layers[UI_RECT(3)].buf.stride[0],
		comp->panel->te.mode, comp->panel->te.pulse_width,
		comp->dither.enable, comp->dither.red_bitdepth,
		comp->dither.gleen_bitdepth, comp->dither.blue_bitdepth,
		comp->vm.hactive, comp->vm.vactive,
		comp->vm.hfront_porch, comp->vm.hback_porch,
		comp->vm.vfront_porch, comp->vm.vback_porch,
		comp->vm.hsync_len, comp->vm.vsync_len,
		comp->vm.pixelclock);

	return strlen(buf);
}
static DEVICE_ATTR_RO(display);

static ssize_t color_bar_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct aic_de_comp *comp = dev_get_drvdata(dev);
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	de_config_update_enable(comp->regs, 0);

	if (enable)
		de_colorbar_ctrl(comp->regs, 1);
	else
		de_colorbar_ctrl(comp->regs, 0);

	de_config_update_enable(comp->regs, 1);

	return size;
}
static DEVICE_ATTR_WO(color_bar);

static ssize_t
dither_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aic_de_comp *comp = dev_get_drvdata(dev);
	struct aic_de_dither *dither = &comp->dither;
	u32 out_depth;

	out_depth = dither->enable ?  dither->red_bitdepth +
			dither->gleen_bitdepth + dither->blue_bitdepth : 24;

	return sprintf(buf, "Dither: %s, Output depth: %d\n",
			dither->enable ? "Enable" : "Disable",
			out_depth);
}

static ssize_t dither_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct aic_de_comp *comp = dev_get_drvdata(dev);
	struct aic_panel *panel = comp->panel;
	u32 val;
	int ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	switch (val) {
	case 16:
		comp->dither.red_bitdepth = 5;
		comp->dither.gleen_bitdepth = 6;
		comp->dither.blue_bitdepth = 5;
		comp->dither.enable = 1;
		break;
	case 18:
		comp->dither.red_bitdepth = 6;
		comp->dither.gleen_bitdepth = 6;
		comp->dither.blue_bitdepth = 6;
		comp->dither.enable = 1;
		break;
	case 0:
	case 24:
		memset(&comp->dither, 0, sizeof(struct aic_de_dither));
		break;
	default:
		pr_err("Invalid output depth, 16/18/24\n");
		return size;
	}

	de_config_update_enable(comp->regs, 0);
	de_set_dither(comp->regs,
		      comp->dither.red_bitdepth,
		      comp->dither.gleen_bitdepth,
		      comp->dither.blue_bitdepth,
		      comp->dither.enable);
	de_config_update_enable(comp->regs, 1);

	panel->disp_dither = val;
	return size;
}
static DEVICE_ATTR_RW(dither);

static struct attribute *aic_de_attrs[] = {
	&dev_attr_display.attr,
	&dev_attr_color_bar.attr,
	&dev_attr_dither.attr,
	NULL
};

static const struct attribute_group aic_de_attr_group = {
	.attrs = aic_de_attrs,
	.name = "debug",
};

static void aic_de_unbind(struct device *dev, struct device *master,
			  void *data)
{
	sysfs_remove_group(&dev->kobj, &aic_de_attr_group);
}

static int aic_de_register_funcs(struct aic_de_comp *comp)
{
	comp->funcs.set_mode = aic_de_set_mode;
	comp->funcs.clk_enable = aic_de_clk_enable;
	comp->funcs.clk_disable = aic_de_clk_disable;
	comp->funcs.timing_enable = aic_de_timing_enable;
	comp->funcs.timing_disable = aic_de_timing_disable;
	comp->funcs.get_layer_num = aic_de_get_layer_num;
	comp->funcs.get_layer_cap = aic_de_get_layer_cap;
	comp->funcs.get_layer_config = aic_de_get_layer_config;
	comp->funcs.update_layer_config = aic_de_update_layer_config;
	comp->funcs.update_layer_config_list = aic_de_update_layer_config_list;
	comp->funcs.shadow_reg_ctrl = aic_de_shadow_reg_ctrl;
	comp->funcs.get_alpha_config = aic_de_get_alpha_config;
	comp->funcs.update_alpha_config = aic_de_update_alpha_config;
	comp->funcs.get_ck_config = aic_de_get_ck_config;
	comp->funcs.update_ck_config = aic_de_update_ck_config;
	comp->funcs.wait_for_vsync = aic_de_wait_for_vsync;
#ifdef CONFIG_DMA_SHARED_BUFFER
	comp->funcs.add_dmabuf = aic_de_add_dmabuf;
	comp->funcs.remove_dmabuf = aic_de_remove_dmabuf;
	comp->funcs.release_dmabuf = aic_de_release_dmabuf;
#endif
	comp->funcs.color_bar_ctrl = aic_de_color_bar_ctrl;
	comp->funcs.pixclk_rate = aic_de_pixclk_rate;
	comp->funcs.pixclk_enable = aic_de_pixclk_enable;
	comp->funcs.set_display_prop = aic_de_set_display_prop;
	comp->funcs.get_display_prop = aic_de_get_display_prop;
	return 0;
}

static int aic_de_bind(struct device *dev, struct device *master,
		       void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aic_de_comp *comp;
	struct resource *res;
	void __iomem *regs;
	int ret;

	comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
	if (!comp)
		return -ENOMEM;

	comp->dev = dev;
	comp->config = of_device_get_match_data(dev);

	dev_set_drvdata(dev, comp);
	g_aic_de_comp = comp;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	comp->regs = regs;

	comp->mclk = devm_clk_get(dev, "de0");
	if (IS_ERR(comp->mclk)) {
		dev_err(dev, "Couldn't get de0 clock\n");
		return PTR_ERR(comp->mclk);
	}

	comp->pclk = devm_clk_get(dev, "pix");
	if (IS_ERR(comp->pclk)) {
		dev_err(dev, "Couldn't get pix clock\n");
		return PTR_ERR(comp->pclk);

	}
	comp->reset = devm_reset_control_get(dev, "de0");
	if (IS_ERR(comp->reset)) {
		dev_err(dev, "Couldn't get reset line\n");
		return PTR_ERR(comp->reset);
	}

	ret = aic_de_parse_dt(dev);
	if (ret)
		return ret;

	ret = aic_de_request_irq(dev, comp);
	if (ret) {
		dev_err(dev, "Couldn't request disp engine IRQ\n");
		return ret;
	}

	ret = sysfs_create_group(&dev->kobj, &aic_de_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create %s node.\n",
				aic_de_attr_group.name);
		return ret;
	}

	/* initialize the vsync wait queue */
	init_waitqueue_head(&comp->vsync_wait);
	spin_lock_init(&comp->slock);
	INIT_LIST_HEAD(&comp->dma_buf);

	/* set display properties to default value */
	comp->disp_prop.bright = 50;
	comp->disp_prop.contrast = 50;
	comp->disp_prop.saturation = 50;
	comp->disp_prop.hue = 50;

	aic_de_register_funcs(comp);
	return 0;
}

static const struct component_ops aic_de_com_ops = {
	.bind	= aic_de_bind,
	.unbind	= aic_de_unbind,
};

static int aic_de_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s()\n", __func__);
	return component_add(&pdev->dev, &aic_de_com_ops);
}

static int aic_de_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &aic_de_com_ops);
	return 0;
}

static const struct aicfb_layer_num layer_num = {
	.vi_num = 1,
	.ui_num = 1,
};

static const struct aicfb_layer_capability aicfb_layer_cap[] = {
	{0, AICFB_LAYER_TYPE_VIDEO, 2048, 2048, AICFB_CAP_SCALING_FLAG},
	{1, AICFB_LAYER_TYPE_UI, 2048, 2048,
	AICFB_CAP_4_RECT_WIN_FLAG|AICFB_CAP_ALPHA_FLAG|AICFB_CAP_CK_FLAG},
};

static const struct aic_de_configs aic_de_cfg = {
	.layer_num = &layer_num,
	.cap = aicfb_layer_cap,
};

static const struct of_device_id aic_de_match_table[] = {
	{.compatible = "artinchip,aic-de-v1.0",
	 .data = &aic_de_cfg},
	{},
};

MODULE_DEVICE_TABLE(of, aic_de_match_table);

static struct platform_driver aic_de_driver = {
	.probe = aic_de_probe,
	.remove = aic_de_remove,
	.driver = {
		.name = "disp_engine",
		.of_match_table	= aic_de_match_table,
	},
};

module_platform_driver(aic_de_driver);

MODULE_AUTHOR("Ning Fang <ning.fang@artinchip.com>");
MODULE_DESCRIPTION("AIC disp engine driver");
MODULE_ALIAS("platform:disp engine");
MODULE_LICENSE("GPL v2");
