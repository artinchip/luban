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
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/console.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/of_address.h>
#include <linux/memblock.h>
#include <linux/component.h>
#include <linux/dma-buf.h>
#include <linux/pm_runtime.h>

#include "video/artinchip_fb.h"
#include "aic_com.h"
#include "hw/de_hw.h"

#define AICFB_NAME "aicfb"
#define AICFB_ON	1
#define AICFB_OFF	0

#define MAX_FB_NUM 1

enum aicfb_port_dir {
	AICFB_PORT_IN,
	AICFB_PORT_OUT
};

struct aicfb_format {
	const char *name;
	u32 bits_per_pixel;
	struct fb_bitfield red;
	struct fb_bitfield green;
	struct fb_bitfield blue;
	struct fb_bitfield transp;
	enum mpp_pixel_format format;
};

struct aicfb_dt {
	u32 rotation;
	u32 disp_buf_num;

	u32 width;
	u32 height;
	u32 width_virtual;
	u32 height_virtual;
	u32 stride;
	u32 size; /* The final framebuffer size to be requested */
	struct aicfb_format *format;

	struct aicfb_disp_prop disp_prop;

	struct device_node *fb_np; /* node pointer of framebuffer */
	struct device_node *de_np; /* node pointer of display engine */
	struct device_node *di_np; /* node pointer of display interface */
	struct device_node *panel_np; /* node pointer of panel */
};

struct aicfb_data {
	struct fb_info *info[MAX_FB_NUM];
	struct device *dev;
	struct aicfb_dt dt_lists[MAX_FB_NUM];
	bool fb_valid[MAX_FB_NUM];
};

struct aicfb_info {
	char name[8];
	int blank;

	int fb_rotate;
	int disp_buf_num;

	int fb_size;
	void *fb_start;
	dma_addr_t fb_start_dma;

	struct mpp_size screen_size;
	u32 pseudo_palette[16];

	struct device *de_dev;
	struct device *di_dev;
	struct device *panel_dev;
	struct de_funcs *de;
	struct di_funcs *di;
	struct aic_panel *panel;

	struct mutex mutex;

	struct aicfb_disp_prop *disp_prop;
	bool set_hsbc;
};

struct aicfb_ioctl_cmd {
	u32 cmd;
	s32 (*f)(struct aicfb_info *fbi, unsigned long arg);
	char desc[32];
};

struct aicfb_format aicfb_format_lists[] = {
	{"a8r8g8b8", 32, {16, 8}, {8, 8}, {0, 8}, {24, 8}, MPP_FMT_ARGB_8888},
	{"a8b8g8r8", 32, {0, 8}, {8, 8}, {16, 8}, {24, 8}, MPP_FMT_ABGR_8888},
	{"x8r8g8b8", 32, {16, 8}, {8, 8}, {0, 8}, {0, 0}, MPP_FMT_XRGB_8888},
	{"r8g8b8", 24, {16, 8}, {8, 8}, {0, 8}, {0, 0}, MPP_FMT_RGB_888},
	{"r5g6b5", 16, {11, 5}, {5, 6}, {0, 5}, {0, 0}, MPP_FMT_RGB_565},
	{"a1r5g5b5", 16, {10, 5}, {5, 5}, {0, 5}, {15, 1}, MPP_FMT_ARGB_1555},
};

phys_addr_t uboot_logo_base;
phys_addr_t uboot_logo_size;
static int uboot_logo_on;

static int __init aicfb_uboot_mem_free(void)
{
	int err;

	if (uboot_logo_size) {
		void *start = phys_to_virt(uboot_logo_base);
		void *end = phys_to_virt(uboot_logo_base + uboot_logo_size);

		err = memblock_free(uboot_logo_base, uboot_logo_size);
		if (err < 0) {
			pr_err("%s: Freeing memblock failed: %d\n",
				__func__, err);
			return err;
		}
		free_reserved_area(start, end, -1, "logo");
	}
	return 0;
}
late_initcall(aicfb_uboot_mem_free);

static void aicfb_videomode_to_fbdata(struct aicfb_dt *dt, struct videomode *vm)
{
	struct aicfb_format *f = dt->format;

	if (!dt->width)
		dt->width = vm->hactive;

	if (!dt->height)
		dt->height = vm->vactive;

	if (dt->rotation == 90 || dt->rotation == 270) {
		u32 tmp = dt->width;

		dt->width = dt->height;
		dt->height = tmp;
	}

	if (dt->width_virtual < dt->width)
		dt->width_virtual = dt->width;

	if (dt->height_virtual < dt->height)
		dt->height_virtual = dt->height;

	if (dt->rotation && !dt->disp_buf_num) {
		if (dt->height_virtual == dt->height)
			dt->disp_buf_num = 1;
		else
			dt->disp_buf_num = 2;
	}

	if (!dt->stride)
		dt->stride = ALIGN_8B(dt->width_virtual * f->bits_per_pixel / 8);

	dt->size = dt->height_virtual * dt->stride;
}

static void aicfb_format_to_var(struct fb_var_screeninfo *var,
		struct aicfb_dt *dt)
{
	struct aicfb_format *f = dt->format;

	var->bits_per_pixel = f->bits_per_pixel;
	var->red = f->red;
	var->green = f->green;
	var->blue = f->blue;
	var->transp = f->transp;

	var->yres = dt->height;
	var->yres_virtual = dt->height_virtual;

	var->xres = dt->width;
	var->xres_virtual = dt->width_virtual;

	var->rotate = dt->rotation;
}

static inline bool need_set_hsbc(struct aicfb_disp_prop *disp_prop)
{
	if (disp_prop->bright != 50 ||
	    disp_prop->contrast != 50 ||
	    disp_prop->saturation != 50 ||
	    disp_prop->hue != 50)
		return true;

	return false;
}

static inline bool check_rotation_degree(u32 degree)
{
	switch (degree) {
	case 0:
	case 90:
	case 180:
	case 270:
		return true;
	default:
		break;
	};

	pr_err("Invalid rotation degree\n");
	return false;
}

static int aic_fb_open(struct fb_info *info, int user)
{
	return 0;
}

static int aic_fb_release(struct fb_info *info, int user)
{
	struct aicfb_info *fbi = (struct aicfb_info *)info->par;

	return fbi->de->release_dmabuf();
}

static unsigned int fb_index;

static int aic_fb_rotate(struct fb_info *info, struct fb_var_screeninfo *var,
		struct aicfb_layer_data *layer, unsigned int offset)
{
	struct aicfb_info *fbi = (struct aicfb_info *)info->par;
	struct ge_bitblt blt = {0};
	s32 ret = 0;

	if (!check_rotation_degree(var->rotate))
		return -EINVAL;

	/* source buffer */
	blt.src_buf.buf_type = MPP_PHY_ADDR;
	blt.src_buf.phy_addr[0] = fbi->fb_start_dma + offset;
	blt.src_buf.stride[0] = info->fix.line_length;
	blt.src_buf.size.width = info->var.xres;
	blt.src_buf.size.height = info->var.yres;
	blt.src_buf.format = layer->buf.format;

	/* destination buffer */
	blt.dst_buf.buf_type = MPP_PHY_ADDR;
	blt.dst_buf.phy_addr[0] = layer->buf.phy_addr[0];
	blt.dst_buf.stride[0] = layer->buf.stride[0];
	blt.dst_buf.size.width = layer->buf.size.width;
	blt.dst_buf.size.height = layer->buf.size.height;
	blt.dst_buf.format = layer->buf.format;

	mutex_lock(&fbi->mutex);
	fbi->fb_rotate = var->rotate;
	mutex_unlock(&fbi->mutex);

	switch (fbi->fb_rotate) {
	case 0:
		blt.ctrl.flags = 0;
		break;
	case 90:
		blt.ctrl.flags = MPP_ROTATION_90;
		break;
	case 180:
		blt.ctrl.flags = MPP_ROTATION_180;
		break;
	case 270:
		blt.ctrl.flags = MPP_ROTATION_270;
		break;
	default:
		pr_err("Invalid rotation degree\n");
		return -EINVAL;
	};

	if (fbi->set_hsbc) {
		u32 csc_coef[12];
		int bright;
		int contrast;
		int sat;
		int hue;

		mutex_lock(&fbi->mutex);
		bright = fbi->disp_prop->bright;
		contrast = fbi->disp_prop->contrast;
		sat = fbi->disp_prop->saturation;
		hue = fbi->disp_prop->hue;
		mutex_unlock(&fbi->mutex);

		get_rgb_hsbc_csc_coefs(bright, contrast, sat, hue, csc_coef);
		ret = aic_ge_bitblt_with_hsbc(&blt, csc_coef);
	} else {
		ret = aic_ge_bitblt(&blt);
	}

	return ret;
}

static int aic_fb_pan_display(struct fb_var_screeninfo *var,
		struct fb_info *info)
{
	struct aicfb_info *fbi = (struct aicfb_info *)info->par;
	struct aicfb_layer_data layer = {0};
	struct de_funcs *de = fbi->de;
	u32 offset;

	offset = var->xoffset * info->var.bits_per_pixel / 8 +
		var->yoffset * ALIGN_8B(info->var.bits_per_pixel / 8 * var->xres);

	layer.layer_id = AICFB_LAYER_TYPE_UI;
	layer.rect_id = 0;
	de->get_layer_config(&layer);

	layer.enable = 1;

	if (var->reserved[0]) {
		layer.buf.fd[0] = var->reserved[0];
		layer.buf.flags = MPP_BUF_PAN_DISPLAY_DMABUF;
		return de->update_layer_config(&layer);
	}

	layer.buf.flags = 0;
	layer.buf.phy_addr[0] = fbi->fb_start_dma + offset;

	if (fbi->fb_rotate || fbi->set_hsbc) {
		layer.buf.phy_addr[0] = fbi->fb_start_dma + fbi->fb_size + offset;

		if (fbi->disp_buf_num == 2 &&
				info->var.yres == info->var.yres_virtual) {
			layer.buf.phy_addr[0] += fbi->fb_size * fb_index;
			fb_index = !fb_index;
		}

		if (aic_fb_rotate(info, var, &layer, offset))
			pr_err("GE Bitblt FB error\n");
	}

	de->update_layer_config(&layer);

	return 0;
}

static s32 aicfb_ioctl_wait_for_vsync(struct aicfb_info *fbi, unsigned long arg)
{
	return fbi->de->wait_for_vsync();
}

static s32 aicfb_ioctl_get_layer_num(struct aicfb_info *fbi, unsigned long arg)
{
	struct aicfb_layer_num num;

	fbi->de->get_layer_num(&num);
	if (copy_to_user((void __user *)arg, &num,
			sizeof(struct aicfb_layer_num)))
		return -EFAULT;

	return 0;
}

static s32 aicfb_ioctl_get_layer_cap(struct aicfb_info *fbi, unsigned long arg)
{
	s32 ret = 0;
	struct aicfb_layer_capability cap;

	if (copy_from_user(&cap,
		(void __user *)arg, sizeof(struct aicfb_layer_capability)))
		return -EFAULT;

	ret = fbi->de->get_layer_cap(&cap);
	if (ret)
		return ret;
	if (copy_to_user((void __user *)arg, &cap,
			sizeof(struct aicfb_layer_capability)))
		return -EFAULT;

	return 0;
}

static s32 aicfb_ioctl_get_layer_cfg(struct aicfb_info *fbi, unsigned long arg)
{
	s32 ret = 0;
	struct aicfb_layer_data layer;

	if (copy_from_user(&layer,
		(void __user *)arg, sizeof(struct aicfb_layer_data)))
		return -EFAULT;

	ret = fbi->de->get_layer_config(&layer);
	if (ret)
		return ret;
	if (copy_to_user((void __user *)arg,
			&layer, sizeof(struct aicfb_layer_data)))
		return -EFAULT;

	return 0;
}

static s32 aicfb_ioctl_update_layer_cfg(struct aicfb_info *fbi,
	unsigned long arg)
{
	struct aicfb_layer_data layer;

	if (copy_from_user(&layer,
		(void __user *)arg, sizeof(struct aicfb_layer_data)))
		return -EFAULT;

	return fbi->de->update_layer_config(&layer);
}

static s32 aicfb_ioctl_set_display_prop(struct aicfb_info *fbi,
	unsigned long arg)
{
	s32 ret = 0;

	mutex_lock(&fbi->mutex);
	if (copy_from_user(fbi->disp_prop,
			   (void __user *)arg,
			   sizeof(struct aicfb_disp_prop))) {
		mutex_unlock(&fbi->mutex);
		return -EFAULT;
	}

	ret = fbi->de->set_display_prop(fbi->disp_prop);
	mutex_unlock(&fbi->mutex);

	return ret;
}

static s32 aicfb_ioctl_get_display_prop(struct aicfb_info *fbi,
	unsigned long arg)
{
	s32 ret = 0;
	struct aicfb_disp_prop prop;

	ret = fbi->de->get_display_prop(&prop);
	if (ret)
		return ret;
	if (copy_to_user((void __user *)arg,
			&prop, sizeof(struct aicfb_disp_prop)))
		return -EFAULT;

	return 0;
}

static s32 aicfb_ioctl_update_layer_cfg_list(struct aicfb_info *fbi,
	unsigned long arg)
{
	u32 num;
	s32 ret = 0;
	struct aicfb_config_lists *plist = NULL;

	if (copy_from_user(&num, (void __user *)arg, sizeof(u32)))
		return -EFAULT;
	if (num == 0)
		return -EFAULT;

	plist = vmalloc(sizeof(u32) + sizeof(struct aicfb_layer_data) * num);
	if (!plist)
		return -ENOMEM;

	if (copy_from_user(plist, (void __user *)arg,
		sizeof(u32) + sizeof(struct aicfb_layer_data) * num)) {
		vfree(plist);
		return -EFAULT;
	}

	ret = fbi->de->update_layer_config_list(plist);

	vfree(plist);
	return ret;
}

static s32 aicfb_ioctl_get_alpha_cfg(struct aicfb_info *fbi, unsigned long arg)
{
	s32 ret = 0;
	struct aicfb_alpha_config alpha = {0};

	if (copy_from_user(&alpha,
		(void __user *)arg, sizeof(struct aicfb_alpha_config)))
		return -EFAULT;

	ret = fbi->de->get_alpha_config(&alpha);
	if (ret)
		return ret;
	if (copy_to_user((void __user *)arg,
			&alpha, sizeof(struct aicfb_alpha_config)))
		return -EFAULT;

	return 0;
}

static s32 aicfb_ioctl_update_alpha_cfg(struct aicfb_info *fbi,
	unsigned long arg)
{
	struct aicfb_alpha_config alpha = {0};

	if (copy_from_user(&alpha,
		(void __user *)arg, sizeof(struct aicfb_alpha_config)))
		return -EFAULT;

	return fbi->de->update_alpha_config(&alpha);
}

static s32 aicfb_ioctl_get_ck_cfg(struct aicfb_info *fbi,
	unsigned long arg)
{
	s32 ret = 0;
	struct aicfb_ck_config ck;

	if (copy_from_user(&ck,
		(void __user *)arg, sizeof(struct aicfb_ck_config)))
		return -EFAULT;

	ret = fbi->de->get_ck_config(&ck);
	if (ret)
		return ret;
	if (copy_to_user((void __user *)arg,
			&ck, sizeof(struct aicfb_ck_config)))
		return -EFAULT;

	return 0;
}

static s32 aicfb_ioctl_update_ck_cfg(struct aicfb_info *fbi,
	unsigned long arg)
{
	struct aicfb_ck_config cfg;

	if (copy_from_user(&cfg,
		(void __user *)arg, sizeof(struct aicfb_ck_config)))
		return -EFAULT;

	return fbi->de->update_ck_config(&cfg);
}

static s32 aicfb_ioctl_get_screen_size(struct aicfb_info *fbi,
	unsigned long arg)
{
	struct mpp_size s;

	memcpy(&s, &fbi->screen_size, sizeof(struct mpp_size));
	if (copy_to_user((void __user *)arg, &s, sizeof(struct mpp_size)))
		return -EFAULT;

	return 0;
}

static s32 aicfb_ioctl_get_fb_layer_cfg(struct aicfb_info *fbi,
	unsigned long arg)
{
	s32 ret = 0;
	struct aicfb_layer_data layer = {0};

	layer.layer_id = 1;
	ret = fbi->de->get_layer_config(&layer);
	if (ret)
		return ret;
	if (copy_to_user((void __user *)arg,
			&layer, sizeof(struct aicfb_layer_data)))
		return -EFAULT;

	return 0;
}

#ifdef CONFIG_DMA_SHARED_BUFFER
static s32 aicfb_ioctl_add_dmabuf(struct aicfb_info *fbi,
	unsigned long arg)
{
	int ret = 0;
	struct dma_buf_info fds;

	if (copy_from_user(&fds,
		(void __user *)arg, sizeof(struct dma_buf_info)))
		return -EFAULT;

	ret = fbi->de->add_dmabuf(&fds);
	if (ret)
		return ret;

	if (copy_to_user((void __user *)arg,
			&fds, sizeof(struct dma_buf_info)))
		return -EFAULT;

	return 0;
}

static s32 aicfb_ioctl_remove_dmabuf(struct aicfb_info *fbi,
	unsigned long arg)
{
	struct dma_buf_info fds;

	if (copy_from_user(&fds,
		(void __user *)arg, sizeof(struct dma_buf_info)))
		return -EFAULT;

	return fbi->de->remove_dmabuf(&fds);
}

dma_addr_t fb_dma_addr;
unsigned int fb_dma_len;

static int aicfb_dma_buf_attach(struct dma_buf *dmabuf,
			   struct dma_buf_attachment *attachment)
{
	return 0;
}

static void aicfb_dma_buf_detach(struct dma_buf *dmabuf,
			    struct dma_buf_attachment *attachment)
{
}

static
struct sg_table *aicfb_map_dma_buf(struct dma_buf_attachment *attachment,
				      enum dma_data_direction direction)
{
	struct sg_table *table;

	table = kmalloc(sizeof(*table), GFP_KERNEL);

	sg_alloc_table(table, 1, GFP_KERNEL);
	sg_dma_len(table->sgl) = fb_dma_len;
	sg_dma_address(table->sgl) = fb_dma_addr;

	return table;
}

static void aicfb_unmap_dma_buf(struct dma_buf_attachment *attachment,
				   struct sg_table *table,
				   enum dma_data_direction direction)
{
	sg_free_table(table);
	kfree(table);
}

static int aicfb_dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	return 0;
}

static void aicfb_dma_buf_release(struct dma_buf *dmabuf)
{
	kfree(dmabuf);
}

static int aicfb_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					     enum dma_data_direction direction)
{
	return 0;
}

static int aicfb_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					   enum dma_data_direction direction)
{
	return 0;
}

static const struct dma_buf_ops aicfb_dmabuf_ops = {
	.map_dma_buf = aicfb_map_dma_buf,
	.unmap_dma_buf = aicfb_unmap_dma_buf,
	.mmap = aicfb_dma_buf_mmap,
	.release = aicfb_dma_buf_release,
	.attach = aicfb_dma_buf_attach,
	.detach = aicfb_dma_buf_detach,
	.begin_cpu_access = aicfb_dma_buf_begin_cpu_access,
	.end_cpu_access = aicfb_dma_buf_end_cpu_access,
};

static s32 aicfb_ioctl_to_dmabuf_fd(struct aicfb_info *fbi, unsigned long arg)
{
	int fd;
	struct dma_buf_info fds;
	struct dma_buf *dmabuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &aicfb_dmabuf_ops;
	exp_info.flags = O_CLOEXEC;
	exp_info.size = fb_dma_len;
	exp_info.priv = &fb_dma_addr;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf))
		return -EFAULT;

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0) {
		dma_buf_put(dmabuf);
		return fd;
	}
	fds.fd = fd;

	if (copy_to_user((void __user *)arg,
			&fds, sizeof(struct dma_buf_info)))
		return -EFAULT;

	return 0;
}
#endif

struct aicfb_ioctl_cmd aicfb_ioctl_cmds[] = {
{AICFB_WAIT_FOR_VSYNC, aicfb_ioctl_wait_for_vsync, "Wait for vsync"},
{AICFB_GET_LAYER_NUM, aicfb_ioctl_get_layer_num, "Get layer num"},
{AICFB_GET_LAYER_CAPABILITY, aicfb_ioctl_get_layer_cap, "Get layer capability"},
{AICFB_GET_LAYER_CONFIG, aicfb_ioctl_get_layer_cfg, "Get layer cfg"},
{AICFB_UPDATE_LAYER_CONFIG, aicfb_ioctl_update_layer_cfg, "Update layer cfg"},
{AICFB_UPDATE_LAYER_CONFIG_LISTS, aicfb_ioctl_update_layer_cfg_list,
	"Update layer cfg list"},
{AICFB_GET_ALPHA_CONFIG, aicfb_ioctl_get_alpha_cfg, "Get alpha cfg"},
{AICFB_UPDATE_ALPHA_CONFIG, aicfb_ioctl_update_alpha_cfg, "Update alpha cfg"},
{AICFB_GET_CK_CONFIG, aicfb_ioctl_get_ck_cfg, "Get Color Key cfg"},
{AICFB_UPDATE_CK_CONFIG, aicfb_ioctl_update_ck_cfg, "Update Color Key cfg"},
{AICFB_GET_SCREEN_SIZE, aicfb_ioctl_get_screen_size, "Get screen size"},
{AICFB_GET_FB_LAYER_CONFIG, aicfb_ioctl_get_fb_layer_cfg, "Get FB layer cfg"},
{AICFB_SET_DISP_PROP, aicfb_ioctl_set_display_prop, "Set display prop"},
{AICFB_GET_DISP_PROP, aicfb_ioctl_get_display_prop, "Get display prop"},
#ifdef CONFIG_DMA_SHARED_BUFFER
{AICFB_ADD_DMABUF, aicfb_ioctl_add_dmabuf, "Add dma-buf by given fd"},
{AICFB_RM_DMABUF, aicfb_ioctl_remove_dmabuf, "Remove dma-buf by given fd"},
{AICFB_TO_DMABUF_FD, aicfb_ioctl_to_dmabuf_fd, "Exports a dma-buf fd"},
#endif
{0xFFFF, NULL, ""}
};

int aic_fb_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	struct aicfb_ioctl_cmd *ioctl = aicfb_ioctl_cmds;
	struct aicfb_info *fbi = (struct aicfb_info *)info->par;

	do {
		if ((ioctl->cmd == cmd) && ioctl->f) {
			pr_debug("%s() - ioctl %#x(%s)\n",
				__func__, cmd, ioctl->desc);
			return ioctl->f(fbi, arg);
		}

		ioctl++;
	} while (ioctl->cmd != 0xFFFF);
	pr_err("%s() - Invalid ioctl cmd %#x\n", __func__, cmd);
	return -EINVAL;
}

int aic_fb_check_var(struct fb_var_screeninfo *var,
			    struct fb_info *info)
{
	if (var->xoffset + var->xres > var->xres_virtual)
		return -EINVAL;

	if (var->yoffset + var->yres > var->yres_virtual)
		return -EINVAL;

	return 0;
}

int aic_fb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct aicfb_info *fbi = (struct aicfb_info *)info->par;

	return dma_mmap_coherent(info->dev, vma, fbi->fb_start,
				 fbi->fb_start_dma, fbi->fb_size);
}

struct fb_ops aicfb_ops = {
	.owner = THIS_MODULE,
	.fb_open = aic_fb_open,
	.fb_release = aic_fb_release,
	.fb_pan_display = aic_fb_pan_display,
	.fb_ioctl = aic_fb_ioctl,
	.fb_check_var = aic_fb_check_var,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_mmap = aic_fb_mmap,
};

static void aicfb_fb_info_setup(struct fb_info *info, struct aicfb_data *fdt)
{
	struct aicfb_info *fbi = (struct aicfb_info *)info->par;
	struct aicfb_dt *dt = &fdt->dt_lists[0];

	info->flags = FBINFO_DEFAULT | FBINFO_PARTIAL_PAN_OK |
		FBINFO_HWACCEL_XPAN | FBINFO_HWACCEL_YPAN;
	info->node = 0;
	strcpy(info->fix.id, fbi->name);
	info->fix.type = FB_TYPE_PACKED_PIXELS;
	info->fix.type_aux = 0;
	info->fix.xpanstep = 0;

	if (info->var.yres_virtual == info->var.yres)
		info->fix.ypanstep = 0;
	else
		info->fix.ypanstep = 1;

	info->fix.ywrapstep = 0;
	info->fix.accel = FB_ACCEL_NONE;
	info->fix.smem_start = fbi->fb_start_dma;
	info->fix.smem_len = fbi->fb_size;
	info->fix.visual = FB_VISUAL_TRUECOLOR;
	info->fix.line_length = dt->stride;
	info->fbops = &aicfb_ops;
	info->pseudo_palette = fbi->pseudo_palette;
	info->screen_base = fbi->fb_start;
	info->screen_size = fbi->fb_size;

	fbi->fb_rotate = dt->rotation;
	fbi->disp_buf_num = dt->disp_buf_num;

	if (fbi->fb_rotate == 90 || fbi->fb_rotate == 270) {
		fbi->screen_size.width = dt->height;
		fbi->screen_size.height = dt->width;
	} else {
		fbi->screen_size.width = dt->width;
		fbi->screen_size.height = dt->height;
	}
}

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int aicfb_add_match_component(struct platform_device *pdev,
				     struct component_match **match, u32 id)
{
	struct aicfb_data *fbd = dev_get_drvdata(&pdev->dev);

	/* match de */
	component_match_add(&pdev->dev, match, compare_of,
			    fbd->dt_lists[id].de_np);

	/* match encoder */
	component_match_add(&pdev->dev, match, compare_of,
			    fbd->dt_lists[id].di_np);

	/* match panel */
	component_match_add(&pdev->dev, match, compare_of,
			    fbd->dt_lists[id].panel_np);

	return 0;
}

/**
 * of_get_fb_by_id() - get the fb device node matching a given id
 * @parent: pointer to the parent device node
 * @id: id of the fb
 *
 */
struct device_node *of_get_fb_by_id(struct device_node *parent, u32 id)
{
	struct device_node *fb_node = NULL;

	for_each_child_of_node(parent, fb_node) {
		u32 fb_id = 0;

		if (!of_node_name_eq(fb_node, "fb"))
			continue;
		of_property_read_u32(fb_node, "reg", &fb_id);
		if (id == fb_id)
			break;
	}
	return fb_node;
}

static struct device_node *of_get_remote_by_port(struct device_node *parent)
{
	struct device_node *remote = NULL;
	struct device_node *port;

	for_each_child_of_node(parent, port) {
		if (of_node_name_eq(port, "port")) {
			remote = of_graph_get_remote_port_parent(port->child);
			break;
		}
		of_node_put(port);
	}

	of_node_put(port);
	return remote;
}

static struct device_node *of_get_remote_by_port_id(struct device_node *parent,
						    enum aicfb_port_dir id)
{
	struct device_node *remote = NULL;
	struct device_node *port;

	port = of_graph_get_port_by_id(parent, id);

	if (!port)
		return NULL;

	remote = of_graph_get_remote_port_parent(port->child);
	of_node_put(port);
	return remote;
}

static int aicfb_parse_fb_rotation(struct device_node *np, struct aicfb_dt *dt)
{
	int ret;

	ret = of_property_read_u32(np, "rotation-degress", &dt->rotation);
	if (ret) {
		dt->rotation = 0;
		dt->disp_buf_num = 0;
		return 0;
	}

	if (!check_rotation_degree(dt->rotation))
		return -EINVAL;

	if (of_property_read_u32(np, "disp-buf-num", &dt->disp_buf_num))
		dt->disp_buf_num = 0;

	return 0;
}

static int aicfb_parse_fb_size(struct device_node *np, struct aicfb_dt *dt)
{
	const char *format;
	int i;

	if (of_property_read_u32(np, "width", &dt->width))
		dt->width = 0;

	if (of_property_read_u32(np, "height", &dt->height))
		dt->height = 0;

	if (of_property_read_u32(np, "width-virtual", &dt->width_virtual))
		dt->width_virtual = 0;

	if (of_property_read_u32(np, "height-virtual", &dt->height_virtual))
		dt->height_virtual = 0;

	if (of_property_read_u32(np, "stride", &dt->stride))
		dt->stride = 0;

	dt->format = NULL;
	if (of_property_read_string(np, "format", &format)) {
		dt->format = &aicfb_format_lists[0];
	} else {
		for (i = 0; i < ARRAY_SIZE(aicfb_format_lists); i++) {
			if (strcmp(format, aicfb_format_lists[i].name))
				continue;
			dt->format = &aicfb_format_lists[i];
			break;
		}
	}

	if (!dt->format) {
		pr_err("fb0 invalid format:%s\n", format);
		return -EINVAL;
	}

	return 0;
}

static void aicfb_parse_disp_prop(struct device_node *np, struct aicfb_dt *dt)
{
	if (of_property_read_u32(np, "disp-bright", &dt->disp_prop.bright))
		dt->disp_prop.bright = 50;

	if (of_property_read_u32(np, "disp-contrast", &dt->disp_prop.contrast))
		dt->disp_prop.contrast = 50;

	if (of_property_read_u32(np, "disp-saturation", &dt->disp_prop.saturation))
		dt->disp_prop.saturation = 50;

	if (of_property_read_u32(np, "disp-hue", &dt->disp_prop.hue))
		dt->disp_prop.hue = 50;
}

static int aicfb_parse_dt_by_fb_id(struct platform_device *pdev, u32 id)
{
	struct device_node *root_np = pdev->dev.of_node;
	struct aicfb_data *aicfb = dev_get_drvdata(&pdev->dev);
	struct aicfb_dt *dt = &aicfb->dt_lists[id];
	struct device_node *np, *node;
	int ret;

	if (!root_np) {
		dev_err(&pdev->dev, "Can't find device node\n");
		goto err_fb;
	}

	/* parse fb device node */
	np = of_get_fb_by_id(root_np, id);
	if (!np) {
		dev_err(&pdev->dev, "Can't find fb@%u\n", id);
		goto err_fb;
	}
	dt->fb_np = np;

	/* parse fb param */
	if (aicfb_parse_fb_rotation(np, dt))
		goto err_param;

	if (aicfb_parse_fb_size(np, dt))
		goto err_param;

	aicfb_parse_disp_prop(np, dt);

	/* get de device node */
	dt->de_np = of_get_remote_by_port(np);

	if (!dt->de_np) {
		dev_err(&pdev->dev, "Can't get de%u device node\n", id);
		goto err_de;
	}

	/* get encoder device node */
	dt->di_np =	of_get_remote_by_port_id(dt->de_np, AICFB_PORT_OUT);

	if (!dt->di_np) {
		dev_err(&pdev->dev, "Can't get encoder%u device node\n", id);
		goto err_en;
	}

	/* get panel device node */
	dt->panel_np = of_get_remote_by_port_id(dt->di_np, AICFB_PORT_OUT);
	if (!dt->panel_np) {
		dev_err(&pdev->dev, "Can't get panel%u device node\n", id);
		goto err_panel;
	}

	ret = of_property_read_u32(np, "artinchip,uboot-logo-on", &uboot_logo_on);
	if (ret) {
		dev_err(&pdev->dev, "Can't parse uboot-logo-on property\n");
		goto err_logo;
	}

	node = of_find_node_by_name(NULL, "aic-logo");
	if (node) {
		struct resource r;

		ret = of_address_to_resource(node, 0, &r);
		if (ret) {
			dev_err(&pdev->dev, "Can't get resource\n");
			goto err_res;
		}

		if (uboot_logo_on) {
			uboot_logo_base = r.start;
			uboot_logo_size = resource_size(&r);

			dev_info(&pdev->dev,
					"logo: base=%#llx, size=%#llx\n",
					uboot_logo_base, uboot_logo_size);
		}
	}

	return 0;

err_res:
	of_node_put(node);
err_logo:
	of_node_put(dt->panel_np);
	dt->panel_np = NULL;
err_panel:
	of_node_put(dt->di_np);
	dt->di_np = NULL;
err_en:
	of_node_put(dt->de_np);
	dt->de_np = NULL;
err_de:
err_param:
	of_node_put(dt->fb_np);
	dt->fb_np = NULL;
err_fb:
	return -EINVAL;
}

static int aicfb_alloc_fb(struct device *dev, u32 id)
{
	struct aicfb_data *fbd = dev_get_drvdata(dev);
	struct aicfb_info *fbi;

	fbd->info[id] = framebuffer_alloc(sizeof(struct aicfb_info), dev);
	if (!fbd->info[id])
		return -ENOMEM;

	fbi = (struct aicfb_info *)fbd->info[id]->par;
	sprintf(fbi->name, "%s%d", AICFB_NAME, id);
	return 0;
}

static int aicfb_find_panel(struct aicfb_data *fbd, u32 id)
{
	struct platform_device *panel_pdev;
	struct aicfb_info *fbi;

	panel_pdev = of_find_device_by_node(fbd->dt_lists[id].panel_np);
	if (!panel_pdev) {
		dev_err(fbd->dev, "Failed to find panel node for fb%d\n", id);
		return -EINVAL;
	}

	fbi = (struct aicfb_info *)fbd->info[id]->par;
	fbi->panel = dev_get_drvdata(&panel_pdev->dev);
	fbi->panel_dev = get_device(&panel_pdev->dev);
	return 0;
}

static int aicfb_find_de(struct aicfb_data *fbd, u32 id)
{
	struct platform_device *de_pdev;
	struct aicfb_info *fbi;

	de_pdev = of_find_device_by_node(fbd->dt_lists[id].de_np);
	if (!de_pdev) {
		dev_err(fbd->dev, "Failed to find de node for fb%d\n", id);
		return -EINVAL;
	}

	fbi = (struct aicfb_info *)fbd->info[id]->par;
	fbi->de = dev_get_drvdata(&de_pdev->dev);
	fbi->de_dev = get_device(&de_pdev->dev);
	return 0;
}

static int aicfb_find_di(struct aicfb_data *fbd, u32 id)
{
	struct platform_device *di_pdev;
	struct aicfb_info *fbi;

	di_pdev = of_find_device_by_node(fbd->dt_lists[id].di_np);
	if (!di_pdev) {
		dev_err(fbd->dev, "Failed to find di node for fb%d\n", id);
		return -EINVAL;
	}

	fbi = (struct aicfb_info *)fbd->info[id]->par;
	fbi->di = dev_get_drvdata(&di_pdev->dev);
	fbi->di_dev = get_device(&di_pdev->dev);
	return 0;
}

static void aicfb_get_panel_info(struct aicfb_info *fbi,
				struct videomode **vm)
{
	struct de_funcs *de = fbi->de;
	struct di_funcs *di = fbi->di;
	struct aic_panel *panel = fbi->panel;

	panel->funcs->get_video_mode(panel, vm);
	de->set_mode(panel, *vm);
	di->attach_panel(panel);
}

static void aicfb_register_panel_callback(struct aicfb_info *fbi)
{
	struct aic_panel *p = fbi->panel;
	struct aic_panel_callbacks cb;

	cb.di_enable = fbi->di->enable;
	cb.di_disable = fbi->di->disable;
	cb.di_send_cmd = fbi->di->send_cmd;
	cb.di_set_videomode = fbi->di->set_videomode;
	cb.timing_enable = fbi->de->timing_enable;
	cb.timing_disable = fbi->de->timing_disable;
	p->funcs->register_callback(fbi->panel, &cb);
}

static void aicfb_enable_panel(struct aicfb_info *fbi, u32 on)
{
	struct aic_panel *p = fbi->panel;

	if (on == AICFB_ON) {
		p->funcs->prepare(p);
		p->funcs->enable(p);
	} else {
		p->funcs->disable(p);
		p->funcs->unprepare(p);
	}
}

static void aicfb_enable_clk(struct aicfb_info *fbi, u32 on)
{
	ulong pixclk;
	struct de_funcs *de = fbi->de;
	struct di_funcs *di = fbi->di;

	if (on == AICFB_ON) {
		de->clk_enable();
		pixclk = de->pixclk_rate();
		di->pixclk2mclk(pixclk);
		di->clk_enable();
		de->pixclk_enable();
	} else {
		di->clk_disable();
		de->clk_disable();
	}
}

static void aicfb_update_alpha(struct aicfb_info *fbi)
{
	struct aicfb_alpha_config alpha = {0};
	struct de_funcs *de = fbi->de;

	alpha.layer_id = AICFB_LAYER_TYPE_UI;
	de->get_alpha_config(&alpha);
	/* TODO: set the alpha config */
	de->update_alpha_config(&alpha);
}

static void aicfb_update_layer(struct aicfb_info *fbi, struct aicfb_dt *dt)
{
	struct aicfb_layer_data layer = {0};
	struct de_funcs *de = fbi->de;

	layer.layer_id = AICFB_LAYER_TYPE_UI;
	layer.rect_id = 0;
	de->get_layer_config(&layer);

	layer.enable = 1;

	switch (fbi->fb_rotate) {
	case 0:
		layer.buf.phy_addr[0] = fbi->fb_start_dma;
		layer.buf.size.width = dt->width;
		layer.buf.size.height = dt->height;
		layer.buf.stride[0] = dt->stride;
		break;
	case 90:
	case 270:
	{
		unsigned int stride = ALIGN_8B(dt->height * dt->format->bits_per_pixel / 8);

		layer.buf.phy_addr[0] = fbi->fb_start_dma + fbi->fb_size;
		layer.buf.size.width = dt->height;
		layer.buf.size.height = dt->width;
		layer.buf.stride[0] = stride;
		break;
	}
	case 180:
		layer.buf.phy_addr[0] = fbi->fb_start_dma + fbi->fb_size;
		layer.buf.size.width = dt->width;
		layer.buf.size.height = dt->height;
		layer.buf.stride[0] = dt->stride;
		break;
	default:
		pr_err("Invalid rotation degree\n");
		return;
	}

	layer.buf.crop_en = 0;
	layer.buf.format = dt->format->format;
	layer.buf.flags = 0;
	de->update_layer_config(&layer);
}

static void aicfb_update_disp_prop(struct aicfb_info *fbi)
{
	struct de_funcs *de = fbi->de;

	de->set_display_prop(fbi->disp_prop);
}

static void aicfb_de_timing_enable(struct aicfb_info *fbi)
{
	struct de_funcs *de = fbi->de;

	de->timing_enable(false);
}

static inline int aicfb_calc_fb_size(struct aicfb_info *fbi,
						struct aicfb_dt *dt)
{
	fbi->fb_size = dt->size;

	if (!dt->rotation && !fbi->set_hsbc)
		return fbi->fb_size;

	if (dt->disp_buf_num == 2 && dt->height_virtual == dt->height)
		return fbi->fb_size * 3;

	return fbi->fb_size * 2;
}

static int aicfb_pm_suspend(struct device *dev);
static int aicfb_pm_resume(struct device *dev);

static ssize_t reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	if (enable) {
		aicfb_pm_suspend(dev);
		aic_delay_ms(20);
		aicfb_pm_resume(dev);
	}

	return size;
}
static DEVICE_ATTR_WO(reset);

static void aicfb_disable_de_di(struct aicfb_info *fbi)
{
	struct aicfb_layer_data layer = {0};
	struct de_funcs *de = fbi->de;
	struct di_funcs *di = fbi->di;

	layer.layer_id = AICFB_LAYER_TYPE_UI;
	layer.rect_id = 0;
	de->get_layer_config(&layer);
	layer.enable = 0;
	de->update_layer_config(&layer);

	di->disable();
	de->timing_disable();
	aicfb_enable_clk(fbi, AICFB_OFF);
}

static void aicfb_enable_de_di(struct aicfb_info *fbi)
{
	struct aicfb_layer_data layer = {0};
	struct de_funcs *de = fbi->de;
	struct di_funcs *di = fbi->di;

	aicfb_enable_clk(fbi, AICFB_ON);

	layer.layer_id = AICFB_LAYER_TYPE_UI;
	layer.rect_id = 0;
	de->get_layer_config(&layer);
	layer.enable = 1;
	de->update_layer_config(&layer);

	di->enable();
	if (di->send_cmd)
		di->send_cmd(0, NULL, 0);
	de->timing_enable(true);
}

static ssize_t reset_de_di_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct aicfb_data *fbd = dev_get_drvdata(dev);
	struct aicfb_info *fbi = (struct aicfb_info *)fbd->info[0]->par;
	bool enable;
	int ret;

	ret = kstrtobool(buf, &enable);
	if (ret)
		return ret;

	if (!enable)
		return size;

	mutex_lock(&fbi->mutex);

	aicfb_disable_de_di(fbi);
	aic_delay_ms(20);
	aicfb_enable_de_di(fbi);

	mutex_unlock(&fbi->mutex);
	return size;
}
static DEVICE_ATTR_WO(reset_de_di);

#define timing_config_attr(field)					\
static ssize_t								\
field##_show(struct device *dev, struct device_attribute *attr,		\
		char *buf)						\
{									\
	struct aicfb_data *fbd = dev_get_drvdata(dev);			\
	struct aicfb_info *fbi = (struct aicfb_info *)fbd->info[0]->par;\
	struct videomode *vm = fbi->panel->vm;				\
									\
	return sprintf(buf, "%d\n", (u32)vm->field);			\
}									\
static ssize_t								\
field##_store(struct device *dev, struct device_attribute *attr,	\
		const char *buf, size_t size)				\
{									\
	struct aicfb_data *fbd = dev_get_drvdata(dev);			\
	struct aicfb_info *fbi = (struct aicfb_info *)fbd->info[0]->par;\
	struct aic_panel *panel = fbi->panel;				\
	struct de_funcs *de = fbi->de;					\
	u32 field;							\
	int ret;							\
									\
	ret = kstrtou32(buf, 0, &field);				\
	if (ret)							\
		return ret;						\
									\
	panel->vm->field = field;					\
	de->set_mode(panel, panel->vm);					\
									\
	return size;							\
}									\
static DEVICE_ATTR_RW(field);

timing_config_attr(pixelclock);
timing_config_attr(hactive);
timing_config_attr(hfront_porch);
timing_config_attr(hback_porch);
timing_config_attr(hsync_len);
timing_config_attr(vactive);
timing_config_attr(vfront_porch);
timing_config_attr(vback_porch);
timing_config_attr(vsync_len);

static struct attribute *aic_fb_attrs[] = {
	&dev_attr_reset.attr,
	&dev_attr_reset_de_di.attr,
	&dev_attr_pixelclock.attr,
	&dev_attr_hactive.attr,
	&dev_attr_hfront_porch.attr,
	&dev_attr_hback_porch.attr,
	&dev_attr_hsync_len.attr,
	&dev_attr_vactive.attr,
	&dev_attr_vfront_porch.attr,
	&dev_attr_vback_porch.attr,
	&dev_attr_vsync_len.attr,
	NULL
};

static const struct attribute_group aic_fb_attr_group = {
	.attrs = aic_fb_attrs,
	.name = "debug",
};

static int aicfb_bind(struct device *dev)
{
	struct aicfb_data *fbd = dev_get_drvdata(dev);
	struct aicfb_dt *dt = &fbd->dt_lists[0];
	struct aicfb_info *fbi;
	struct videomode *vm;
	u32 fb_size;
	int ret;

	ret = component_bind_all(dev, dev);
	if (ret) {
		dev_err(dev, "component_bind_all failed\n");
		goto err_bind;
	}

	/* alloc fb0_info */
	ret = aicfb_alloc_fb(dev, 0);
	if (ret) {
		dev_err(dev, "alloc fb0_info failed\n");
		goto err_fb0;
	}
	fbi = (struct aicfb_info *)fbd->info[0]->par;

	/* set display properties */
	fbi->disp_prop = &dt->disp_prop;
	fbi->set_hsbc = need_set_hsbc(fbi->disp_prop);

	/* Find the component device, and get video mode */
	aicfb_find_de(fbd, 0);
	aicfb_find_di(fbd, 0);
	aicfb_find_panel(fbd, 0);
	aicfb_get_panel_info(fbi, &vm);
	aicfb_register_panel_callback(fbi);

	/* Initialize fb_var_screeninfo */
	aicfb_videomode_to_fbdata(dt, vm);
	aicfb_format_to_var(&fbd->info[0]->var, dt);

	/* request fb0 framebuffer */
	fb_size = aicfb_calc_fb_size(fbi, dt);
	fbi->fb_start = dma_alloc_coherent(dev, PAGE_ALIGN(fb_size),
				&fbi->fb_start_dma,  GFP_KERNEL);
	if (fbi->fb_start == NULL) {
		ret = -ENOMEM;
		goto err_dma_alloc;
	}
	fb_dma_addr = fbi->fb_start_dma;
	fb_dma_len = fbi->fb_size;

	dev_info(dev, "%d allocated for %s, %#llx/%#llx\n",
		fb_size, fbi->name, (u64)fbi->fb_start, fbi->fb_start_dma);

	aicfb_fb_info_setup(fbd->info[0], fbd);

	ret = fb_alloc_cmap(&fbd->info[0]->cmap, 256, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to alloc cmap\n");
		goto err_cmap;
	}

	/* Turn on the component device */
	aicfb_enable_clk(fbi, AICFB_ON);
	aicfb_update_alpha(fbi);
	aicfb_update_disp_prop(fbi);

	/* Avoid enable panel again */
	if (uboot_logo_on && uboot_logo_size) {
		void *src = phys_to_virt(uboot_logo_base);
		void *dst = NULL;

		if (fbi->fb_rotate)
			dst = phys_to_virt(fbi->fb_start_dma + fbi->fb_size);
		else
			dst = fbi->fb_start;

		memcpy(dst, src, uboot_logo_size);

		/*
		 * When the logo data is ready, update UI layer.
		 * If update UI layer first, the screen will tear.
		 */
		aicfb_update_layer(fbi, dt);

		/* Ensure that display engine interrupts are enabled */
		aicfb_de_timing_enable(fbi);
	} else {
		aicfb_update_layer(fbi, dt);
		aicfb_enable_panel(fbi, AICFB_ON);
	}

	ret = register_framebuffer(fbd->info[0]);
	if (ret < 0) {
		dev_err(dev, "Failed to register fb0: %d\n", ret);
		ret = -ENXIO;
		goto err_register;
	}
	dev_info(dev, "loaded to /dev/fb%d <%s>.\n",
		fbd->info[0]->node, fbd->info[0]->fix.id);

	ret = sysfs_create_group(&dev->kobj, &aic_fb_attr_group);
	if (ret) {
		dev_err(dev, "Failed to create %s node.\n",
				aic_fb_attr_group.name);
		return ret;
	}

	mutex_init(&fbi->mutex);

	return 0;

err_register:
	fb_dealloc_cmap(&fbd->info[0]->cmap);
err_cmap:
	dma_free_coherent(dev, PAGE_ALIGN(fbi->fb_size), fbi->fb_start,
		fbi->fb_start_dma);
err_dma_alloc:
	framebuffer_release(fbd->info[0]);
err_fb0:
	component_unbind_all(dev, dev);
err_bind:
	return ret;
}

static void aicfb_unbind(struct device *dev)
{
	struct aicfb_data *aicfb = dev_get_drvdata(dev);

	component_unbind_all(dev, dev);

	unregister_framebuffer(aicfb->info[0]);
	framebuffer_release(aicfb->info[0]);
	sysfs_remove_group(&dev->kobj, &aic_fb_attr_group);
}

static const struct component_master_ops aic_component_ops = {
	.bind = aicfb_bind,
	.unbind = aicfb_unbind,
};

static int aicfb_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;
	int ret;
	struct aicfb_data *fbd;

	fbd = devm_kzalloc(&pdev->dev, sizeof(*fbd), GFP_KERNEL);
	if (!fbd)
		return -ENOMEM;
	dev_dbg(&pdev->dev, "%s()\n", __func__);

	/* parse fb0 dt */
	dev_set_drvdata(&pdev->dev, fbd);
	ret = aicfb_parse_dt_by_fb_id(pdev, 0);
	if (ret)
		return ret;

	/* add fb0 component */
	aicfb_add_match_component(pdev, &match, 0);

	ret = component_master_add_with_match(&pdev->dev,
					      &aic_component_ops, match);
	if (ret) {
		dev_err(&pdev->dev, "component_master_add_with_match failed\n");
		return ret;
	}

	fbd->dev = &pdev->dev;
	return 0;
}

void aicfb_release_device_node(struct device *dev, u32 id)
{
	struct aicfb_data *fbd = dev_get_drvdata(dev);
	struct aicfb_info *fbi = (struct aicfb_info *)fbd->info[id]->par;

	put_device(fbi->de_dev);
	put_device(fbi->di_dev);
	put_device(fbi->panel_dev);
	of_node_put(fbd->dt_lists[id].fb_np);
	of_node_put(fbd->dt_lists[id].de_np);
	of_node_put(fbd->dt_lists[id].di_np);
	of_node_put(fbd->dt_lists[id].panel_np);
}

static int aicfb_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &aic_component_ops);
	aicfb_release_device_node(&pdev->dev, 0);
	return 0;
}

#ifdef CONFIG_PM
static int aicfb_pm_suspend(struct device *dev)
{
	struct aicfb_data *fbd = dev_get_drvdata(dev);
	struct aicfb_info *fbi = (struct aicfb_info *)fbd->info[0]->par;
	struct aicfb_layer_data layer = {0};
	struct de_funcs *de = fbi->de;

	mutex_lock(&fbi->mutex);
	aicfb_enable_panel(fbi, AICFB_OFF);

	layer.layer_id = AICFB_LAYER_TYPE_UI;
	layer.rect_id = 0;
	de->get_layer_config(&layer);

	layer.enable = 0;
	de->update_layer_config(&layer);

	aicfb_enable_clk(fbi, AICFB_OFF);
	mutex_unlock(&fbi->mutex);

	return 0;
}

static int aicfb_pm_resume(struct device *dev)
{
	struct aicfb_data *fbd = dev_get_drvdata(dev);
	struct aicfb_info *fbi = (struct aicfb_info *)fbd->info[0]->par;
	struct aicfb_layer_data layer = {0};
	struct de_funcs *de = fbi->de;

	mutex_lock(&fbi->mutex);
	aicfb_enable_clk(fbi, AICFB_ON);
	aicfb_update_alpha(fbi);

	layer.layer_id = AICFB_LAYER_TYPE_UI;
	layer.rect_id = 0;
	de->get_layer_config(&layer);

	layer.enable = 1;
	de->update_layer_config(&layer);

	aicfb_enable_panel(fbi, AICFB_ON);
	mutex_unlock(&fbi->mutex);

	return 0;
}

static UNIVERSAL_DEV_PM_OPS(aicfb_pm_ops, aicfb_pm_suspend,
		aicfb_pm_resume, NULL);
#endif

static const struct of_device_id aicfb_match_table[] = {
	{.compatible = "artinchip,aic-framebuffer",},
	{},
};

MODULE_DEVICE_TABLE(of, aicfb_match_table);

static struct platform_driver aicfb_driver = {
	.probe = aicfb_probe,
	.remove = aicfb_remove,
	.driver = {
		.name = AICFB_NAME,
		.of_match_table	= aicfb_match_table,
#ifdef CONFIG_PM
		.pm = &aicfb_pm_ops,
#endif
	},
};

module_platform_driver(aicfb_driver);

MODULE_AUTHOR("Ning Fang <ning.fang@artinchip.com>");
MODULE_DESCRIPTION("AIC framebuffer driver");
MODULE_ALIAS("platform:" AICFB_NAME);
MODULE_LICENSE("GPL v2");
