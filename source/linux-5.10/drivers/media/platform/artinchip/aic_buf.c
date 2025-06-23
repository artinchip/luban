// SPDX-License-Identifier: GPL-2.0-only
/*
 * The buffer management of ArtInChip DVP controller driver.
 *
 * Copyright (C) 2020-2022 ArtInChip Technology Co., Ltd.
 * Authors:  Matteo <duanmt@artinchip.com>
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-v4l2.h>

#include "aic_dvp.h"

#define DVP_FIRST_BUF		0
#define BUF_IS_INVALID(index)	(((index) < 0) || ((index) >= DVP_MAX_BUF))

static inline struct aic_dvp_buf *vb2_v4l2_to_dvp_buffer(
					const struct vb2_v4l2_buffer *p)
{
	return container_of(p, struct aic_dvp_buf, vb);
}

static inline struct aic_dvp_buf *vb2_to_dvp_buffer(const struct vb2_buffer *p)
{
	return vb2_v4l2_to_dvp_buffer(to_vb2_v4l2_buffer(p));
}

static int aic_dvp_queue_setup(struct vb2_queue *vq,
			       unsigned int *nbuffers,
			       unsigned int *nplanes,
			       unsigned int sizes[],
			       struct device *alloc_devs[])
{
	struct aic_dvp *dvp = vb2_get_drv_priv(vq);
	unsigned int num_planes = dvp->fmt.num_planes;
	unsigned int i;

	if (*nplanes) {
		if (*nplanes != num_planes)
			return -EINVAL;

		for (i = 0; i < num_planes; i++)
			if (sizes[i] < dvp->fmt.plane_fmt[i].sizeimage)
				return -EINVAL;
		return 0;
	}

	*nplanes = num_planes;
	for (i = 0; i < num_planes; i++)
		sizes[i] = dvp->fmt.plane_fmt[i].sizeimage;

	return 0;
};

static int aic_dvp_buf_prepare(struct vb2_buffer *vb)
{
	struct aic_dvp *dvp = vb2_get_drv_priv(vb->vb2_queue);
	struct aic_dvp_buf *buf = vb2_to_dvp_buffer(vb);
	unsigned int i;

	for (i = 0; i < dvp->fmt.num_planes; i++) {
		unsigned long size = dvp->fmt.plane_fmt[i].sizeimage;

		if (vb2_plane_size(vb, i) < size) {
			dev_err(dvp->dev, "buffer too small (%lu < %lu)\n",
				vb2_plane_size(vb, i), size);
			return -EINVAL;
		}

		vb2_set_plane_payload(vb, i, size);
		buf->paddr[i] = vb2_dma_contig_plane_dma_addr(vb, i);
	}

	return 0;
}

static int aic_dvp_buf_reload(struct aic_dvp *dvp, struct aic_dvp_buf *buf)
{
	struct vb2_v4l2_buffer *vb = &buf->vb;
	int index = vb->vb2_buf.index;

	dev_dbg(dvp->dev, "Set the buf %d to register", index);
	/*
	 * We should never end up in a situation where we overwrite an
	 * already filled slot.
	 */
	if (WARN_ON(dvp->vbuf[index]))
		return -EINVAL;

	dvp->vbuf[index] = vb;

	aic_dvp_update_buf_addr(dvp, buf, 0);
	aic_dvp_update_ctl(dvp);
	return 0;
}

static void aic_dvp_buf_mark_done(struct aic_dvp *dvp,
				  struct vb2_v4l2_buffer *vb,
				  unsigned int sequence, u32 err)
{
	int index = vb->vb2_buf.index;

	dev_dbg(dvp->dev, "Mark the buf %d done", index);
	if (!dvp->vbuf[index]) {
		dev_dbg(dvp->dev, "Buffer %d was empty!\n", index);
		return;
	}

	vb = dvp->vbuf[index];
	vb->field = dvp->fmt.field;
	vb->sequence = sequence;
	vb->vb2_buf.timestamp = ktime_get_ns();
	if (err)
		vb2_buffer_done(&vb->vb2_buf, VB2_BUF_STATE_ERROR);
	else
		vb2_buffer_done(&vb->vb2_buf, VB2_BUF_STATE_DONE);

	dvp->vbuf[index] = NULL;
}

static int aic_dvp_top_field_done(struct aic_dvp *dvp, u32 err)
{
	struct aic_dvp_buf *cur_buf = NULL;

	if (list_empty(&dvp->buf_list)) {
		dev_err(dvp->dev, "%s() - No buf available!\n", __func__);
		return 0;
	}

	cur_buf = list_first_entry(&dvp->buf_list, struct aic_dvp_buf, list);
	dev_dbg(dvp->dev, "%s() - cur: index %d, dvp_using %d\n",
		__func__, cur_buf->vb.vb2_buf.index, cur_buf->dvp_using);
	if (BUF_IS_INVALID(cur_buf->vb.vb2_buf.index)) {
		dev_err(dvp->dev, "%s() - Invalid buf %d\n",
			__func__, cur_buf->vb.vb2_buf.index);
		return -1;
	}

	dev_dbg(dvp->dev, "Add offset %d of cur buf %d",
		dvp->cfg.stride[0], cur_buf->vb.vb2_buf.index);

	aic_dvp_update_buf_addr(dvp, cur_buf, dvp->cfg.stride[0]);
	aic_dvp_update_ctl(dvp);
	dvp->sequence++;
	return 0;
}

static int aic_dvp_frame_done(struct aic_dvp *dvp, u32 err)
{
	struct aic_dvp_buf *cur_buf = NULL;

	if (list_empty(&dvp->buf_list)) {
		dev_err(dvp->dev, "%s() - No buf available!\n", __func__);
		return 0;
	}

	cur_buf = list_first_entry(&dvp->buf_list, struct aic_dvp_buf, list);
	dev_dbg(dvp->dev, "%s() - cur: index %d, dvp_using %d\n",
		__func__, cur_buf->vb.vb2_buf.index, cur_buf->dvp_using);
	if (BUF_IS_INVALID(cur_buf->vb.vb2_buf.index)) {
		dev_err(dvp->dev, "%s() - Invalid buf %d\n",
			__func__, cur_buf->vb.vb2_buf.index);
		return -1;
	}

	/* If cur_buf is a new one queued, DVP should use it first */
	if (!cur_buf->dvp_using) {
		dev_dbg(dvp->dev, "%s() - Good! Buf %d is free again\n",
			__func__, cur_buf->vb.vb2_buf.index);
		cur_buf->dvp_using = true;
		aic_dvp_buf_reload(dvp, cur_buf);
		dvp->sequence++;
		return 0;
	}

	/* Report the current buffer as done */
	list_del(&cur_buf->list);
	aic_dvp_buf_mark_done(dvp, &cur_buf->vb, dvp->sequence, err);

	return 0;
}

static int aic_dvp_update_addr(struct aic_dvp *dvp)
{
	struct aic_dvp_buf *cur_buf;
	struct aic_dvp_buf *next_buf;

	if (!dvp->streaming)
		return 0;

	if (list_empty(&dvp->buf_list)) {
		dev_warn(dvp->dev, "%s() - No buf available!\n", __func__);
		return -1;
	}

	cur_buf = list_first_entry(&dvp->buf_list, struct aic_dvp_buf, list);
	dev_dbg(dvp->dev, "%s() - cur: index %d, dvp_using %d\n",
		__func__, cur_buf->vb.vb2_buf.index, cur_buf->dvp_using);
	if (BUF_IS_INVALID(cur_buf->vb.vb2_buf.index)) {
		dev_err(dvp->dev, "%s() - Cur buf %d is invalid\n",
			__func__, cur_buf->vb.vb2_buf.index);
		return -1;
	}

	if (!cur_buf->dvp_using) {
		cur_buf->dvp_using = true;
		aic_dvp_buf_reload(dvp, cur_buf);
		dvp->sequence++;
		return 0;
	}

	if (cur_buf == list_last_entry(&dvp->buf_list, struct aic_dvp_buf,
				       list)) {
		dev_warn(dvp->dev, "%s() - It's the last buf!\n", __func__);
		return 0;
	}
	next_buf = list_next_entry(cur_buf, list);
	if (!next_buf || BUF_IS_INVALID(next_buf->vb.vb2_buf.index)) {
		dev_err(dvp->dev, "%s() - Next buf is invalid\n", __func__);
		return -1;
	}
	dev_dbg(dvp->dev, "%s() - Next: index %d, dvp_using %d\n",
		__func__, next_buf->vb.vb2_buf.index, next_buf->dvp_using);

	/* DVP can use the next buf as output. */
	if (!next_buf->dvp_using) {
		next_buf->dvp_using = true;
		aic_dvp_buf_reload(dvp, next_buf);
		dvp->sequence++;
	} else {
		/* This should never happened! */
		dev_err(dvp->dev, "%s() - So weird! DVP is using two buf!\n",
			__func__);
		return -1;
	}

	return 0;
}

static void aic_dvp_buf_queue(struct vb2_buffer *vb)
{
	struct aic_dvp *dvp = vb2_get_drv_priv(vb->vb2_queue);
	struct aic_dvp_buf *buf = vb2_to_dvp_buffer(vb);
	unsigned long flags;

	dev_dbg(dvp->dev, "%s() - buf %d\n", __func__, buf->vb.vb2_buf.index);

	spin_lock_irqsave(&dvp->qlock, flags);
	buf->dvp_using = false;
	list_add_tail(&buf->list, &dvp->buf_list);
	spin_unlock_irqrestore(&dvp->qlock, flags);
}

static void aic_dvp_return_all_buffers(struct aic_dvp *dvp,
				       enum vb2_buffer_state state)
{
	int i;
	struct aic_dvp_buf *buf, *node;

	list_for_each_entry_safe(buf, node, &dvp->buf_list, list) {
		vb2_buffer_done(&buf->vb.vb2_buf, state);
		list_del(&buf->list);
	}

	for (i = 0; i < DVP_MAX_BUF; i++) {
		if (!dvp->vbuf[i])
			continue;

		dvp->vbuf[i] = NULL;
	}
}

static int aic_dvp_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct aic_dvp *dvp = vb2_get_drv_priv(vq);
	struct aic_dvp_buf *buf;
	unsigned long flags;
	int ret;

	dev_dbg(dvp->dev, "Starting capture\n");

	dvp->sequence = 0;
	aic_dvp_field_tag_clr();

	ret = media_pipeline_start(&dvp->vdev.entity, &dvp->vdev.pipe);
	if (ret < 0)
		goto err_clear_dma_queue;

	spin_lock_irqsave(&dvp->qlock, flags);

	aic_dvp_set_cfg(dvp);
	aic_dvp_set_pol(dvp);
	writel(0x80000000, dvp->regs + DVP_OUT_FRA_NUM(dvp->ch));

	aic_dvp_clr_int(dvp);
	aic_dvp_enable_int(dvp, 1);

	/* Prepare our buffers in hardware */
	buf = list_first_entry(&dvp->buf_list, struct aic_dvp_buf, list);
	buf->dvp_using = true;
	ret = aic_dvp_buf_reload(dvp, buf);
	if (ret) {
		spin_unlock_irqrestore(&dvp->qlock, flags);
		goto err_disable_pipeline;
	}

	aic_dvp_capture_start(dvp);
	aic_dvp_update_ctl(dvp);
	spin_unlock_irqrestore(&dvp->qlock, flags);

	ret = v4l2_subdev_call(dvp->src_subdev, video, s_stream, 1);
	if (ret < 0 && ret != -ENOIOCTLCMD)
		goto err_disable_device;

	dvp->streaming = 1;
	return 0;

err_disable_device:
	aic_dvp_capture_stop(dvp);
	aic_dvp_update_ctl(dvp);

err_disable_pipeline:
	media_pipeline_stop(&dvp->vdev.entity);

err_clear_dma_queue:
	spin_lock_irqsave(&dvp->qlock, flags);
	aic_dvp_return_all_buffers(dvp, VB2_BUF_STATE_QUEUED);
	spin_unlock_irqrestore(&dvp->qlock, flags);

	return ret;
}

static void aic_dvp_stop_streaming(struct vb2_queue *vq)
{
	struct aic_dvp *dvp = vb2_get_drv_priv(vq);
	unsigned long flags;

	dev_dbg(dvp->dev, "Stopping capture\n");

	v4l2_subdev_call(dvp->src_subdev, video, s_stream, 0);
	aic_dvp_enable_int(dvp, 0);
	aic_dvp_capture_stop(dvp);
	aic_dvp_update_ctl(dvp);

	/* Release all active buffers */
	spin_lock_irqsave(&dvp->qlock, flags);
	aic_dvp_return_all_buffers(dvp, VB2_BUF_STATE_ERROR);
	spin_unlock_irqrestore(&dvp->qlock, flags);

	media_pipeline_stop(&dvp->vdev.entity);
	dvp->streaming = 0;
}

static const struct vb2_ops aic_dvp_qops = {
	.queue_setup		= aic_dvp_queue_setup,
	.buf_prepare		= aic_dvp_buf_prepare,
	.buf_queue		= aic_dvp_buf_queue,
	.start_streaming	= aic_dvp_start_streaming,
	.stop_streaming		= aic_dvp_stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

static irqreturn_t aic_dvp_isr(int irq, void *data)
{
	struct aic_dvp *dvp = data;
	u32 sta, err = 0;
	unsigned long flags;
	static u32 recv_first_field;

	sta = aic_dvp_clr_int(dvp);
	dev_dbg(dvp->dev, "IRQ status %#x, sequence %d\n", sta, dvp->sequence);
	if (sta & DVP_IRQ_STA_BUF_FULL) {
		dev_warn(dvp->dev, "%s() Buf is full! %#x\n", __func__, sta);
		err = 1;
	} else if (sta & DVP_IRQ_STA_XY_CODE_ERR) {
		dev_warn(dvp->dev, "%s() XY code error %#x\n", __func__, sta);
		aic_dvp_clr_fifo(dvp);
		return IRQ_HANDLED;
	}

	if (sta & DVP_IRQ_EN_FRAME_DONE) {
		if (err)
			aic_dvp_clr_fifo(dvp);

		if (V4L2_FIELD_IS_INTERLACED(dvp->cfg.field)) {
			/* If the first field is a bottom field, ignore it */
			if (!recv_first_field && aic_dvp_is_bottom_field()) {
				dev_info(dvp->dev,
					 "The first is bottom field - ignored\n");
				aic_dvp_clr_fifo(dvp);
				recv_first_field = 1;
				return IRQ_HANDLED;
			}

			if (aic_dvp_is_top_field()) {
				recv_first_field = 1;
				return IRQ_HANDLED;
			}
		}

		spin_lock_irqsave(&dvp->qlock, flags);
		if (aic_dvp_frame_done(dvp, err))
			dev_warn(dvp->dev, "%s() - Failed to complete buf\n",
				 __func__);
		spin_unlock_irqrestore(&dvp->qlock, flags);
	}
	if (sta & DVP_IRQ_STA_HNUM) {
		if (V4L2_FIELD_IS_INTERLACED(dvp->cfg.field)) {
			aic_dvp_get_current_xy(dvp);

			if (aic_dvp_is_top_field()) {
				spin_lock_irqsave(&dvp->qlock, flags);
				aic_dvp_top_field_done(dvp, err);
				spin_unlock_irqrestore(&dvp->qlock, flags);
				recv_first_field = 1;
				return IRQ_HANDLED;
			}

			/* If the first field is a bottom field, ignore it */
			if (!recv_first_field) {
				dev_dbg(dvp->dev,
					"The first is bottom field - ignore\n");
				return IRQ_HANDLED;
			}
		}

		spin_lock_irqsave(&dvp->qlock, flags);
		if (aic_dvp_update_addr(dvp))
			dev_warn(dvp->dev, "%s() - Failed to update buf\n",
				 __func__);
		spin_unlock_irqrestore(&dvp->qlock, flags);
	}

	return IRQ_HANDLED;
}

int aic_dvp_buf_register(struct aic_dvp *dvp)
{
	struct vb2_queue *q = &dvp->queue;
	int ret;
	int i;

	spin_lock_init(&dvp->qlock);
	mutex_init(&dvp->lock);

	INIT_LIST_HEAD(&dvp->buf_list);
	for (i = 0; i < DVP_MAX_BUF; i++)
		dvp->vbuf[i] = NULL;

	q->min_buffers_needed = DVP_MAX_BUF;
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_MMAP;
	q->lock = &dvp->lock;
	q->drv_priv = dvp;
	q->buf_struct_size = sizeof(struct aic_dvp_buf);
	q->ops = &aic_dvp_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->dev = dvp->dev;

	ret = vb2_queue_init(q);
	if (ret < 0) {
		dev_err(dvp->dev, "failed to initialize VB2 queue\n");
		goto err_free_mutex;
	}

	ret = v4l2_device_register(dvp->dev, &dvp->v4l2);
	if (ret) {
		dev_err(dvp->dev, "Couldn't register the v4l2 device\n");
		goto err_free_queue;
	}

	ret = devm_request_irq(dvp->dev, dvp->irq, aic_dvp_isr, 0,
			       dev_name(dvp->dev), dvp);
	if (ret) {
		dev_err(dvp->dev, "Couldn't register our interrupt\n");
		goto err_unregister_device;
	}

	return 0;

err_unregister_device:
	v4l2_device_unregister(&dvp->v4l2);

err_free_queue:
	vb2_queue_release(q);

err_free_mutex:
	mutex_destroy(&dvp->lock);
	return ret;
}

void aic_dvp_buf_unregister(struct aic_dvp *dvp)
{
	v4l2_device_unregister(&dvp->v4l2);
	vb2_queue_release(&dvp->queue);
	mutex_destroy(&dvp->lock);
}
