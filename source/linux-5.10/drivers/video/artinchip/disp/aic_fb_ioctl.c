// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co., Ltd.
 * Authors:  Huahui Mai <huahui.mai@artinchip.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>

#include "video/artinchip_fb.h"
#include "aic_fb.h"

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

	fb_dma_addr = fbi->fb_start_dma;
	fb_dma_len = fbi->fb_size;

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

static s32 aicfb_ioctl_get_dma_coherent_status(struct aicfb_info *fbi, unsigned long arg)
{
	bool dma_coherent = fbi->dma_coherent;

	if (copy_to_user((void __user *)arg,
			&dma_coherent, sizeof(bool)))
		return -EFAULT;
	return 0;
}

static s32 aicfb_ioctl_update_dma_coherent_status(struct aicfb_info *fbi, unsigned long arg)
{
	bool dma_coherent;

	if (copy_from_user(&dma_coherent,
		(void __user *)arg, sizeof(bool)))
		return -EFAULT;

	fbi->dma_coherent = dma_coherent;
	return 0;
}
#endif /* CONFIG_DMA_SHARED_BUFFER */

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
{AICFB_GET_DMA_COHERENT_STATUS, aicfb_ioctl_get_dma_coherent_status,
	"Get dma coherent status"},
{AICFB_UPDATE_DMA_COHERENT_STATUS, aicfb_ioctl_update_dma_coherent_status,
	"Update dma coherent status"},
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

