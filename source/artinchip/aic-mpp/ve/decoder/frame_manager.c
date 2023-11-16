/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: frame buffer manager
*/

#define LOG_TAG "frame_manager"

#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include "mpp_mem.h"
#include "ve_buffer.h"
#include "mpp_list.h"
#include "frame_manager.h"
#include "frame_allocator.h"
#include "ve.h"
#include "mpp_log.h"

#define MPP_MAX_FRAME_NUM	(18)

/********************* default frame allocator ******************************/
struct def_frame_allocator {
	struct frame_allocator base;
	int dma_fd;
};

int def_alloc_frame_buffer(struct frame_allocator *p, struct mpp_frame* frame,
		int width, int height, enum mpp_pixel_format format)
{
	struct def_frame_allocator* impl = (struct def_frame_allocator*)p;

	frame->buf.size.height = height;
	frame->buf.stride[0] = width;
	frame->buf.format = format;
	return mpp_buf_alloc(impl->dma_fd, &frame->buf);
}

int def_free_frame_buffer(struct frame_allocator *p, struct mpp_frame *frame)
{
	mpp_buf_free(&frame->buf);

	return 0;
}

int def_close_allocator(struct frame_allocator *p)
{
	struct def_frame_allocator* impl = (struct def_frame_allocator*)p;

	dmabuf_free(impl->dma_fd);
	mpp_free(impl);

	return 0;
}

static struct alloc_ops def_ops = {
	.alloc_frame_buffer = def_alloc_frame_buffer,
	.free_frame_buffer = def_free_frame_buffer,
	.close_allocator = def_close_allocator,
};

struct frame_allocator* def_open_allocator()
{
	struct def_frame_allocator* impl = (struct def_frame_allocator*)mpp_alloc(sizeof(struct def_frame_allocator));
	if(impl == NULL) {
		return NULL;
	}
	memset(impl, 0, sizeof(struct def_frame_allocator));

	impl->dma_fd = dmabuf_device_open();
	impl->base.ops = &def_ops;

	return &impl->base;
}
/********************* default frame allocator end ******************************/

struct frame_impl {
	struct frame frm;

	int used_by_decoder;
	int used_by_display;
	int displayed;				// the frame has been displayed
	int ref_count;				// ref count of this frame

	int node_id;
	int comp_num;				// the number of component(YUV)
	struct mpp_list list;
};

struct frame_manager {
	int frame_count;

	struct frame_allocator *allocator;
	struct frame_impl *frame_node;

	pthread_mutex_t lock;
	int empty_num;				// frame number in empty list
	int render_num;				// frame number in render list
	struct mpp_list empty_list;
	struct mpp_list render_list;
};

static int add_dmabuf(struct frame_impl *frame, int line_stride)
{
	int comp = 0;
	int stride[3] = {0, 0, 0};
	enum mpp_pixel_format pixel_format;
	int i;

	logd("alloc frame buffer");

	if (!frame) {
		return -1;
	}

	pixel_format = frame->frm.mpp_frame.buf.format;

	switch(pixel_format)
	{
	case MPP_FMT_YUV420P:
		comp = 3;
		stride[0] = line_stride;
		stride[1] = stride[2] = line_stride >> 1;

		break;
	case MPP_FMT_YUV444P:
		comp = 3;
		stride[1] = stride[2] = stride[0] = line_stride;
		break;

	case MPP_FMT_YUV422P:
		comp = 3;
		stride[0] = line_stride;
		stride[1] = stride[2] = line_stride >> 1;
		break;

	case MPP_FMT_NV12:
	case MPP_FMT_NV21:
		comp = 2;
		stride[1] = stride[0] = line_stride;
		break;

	case MPP_FMT_YUV400:
	case MPP_FMT_ABGR_8888:
	case MPP_FMT_ARGB_8888:
	case MPP_FMT_RGBA_8888:
	case MPP_FMT_BGRA_8888:
	case MPP_FMT_BGR_888:
	case MPP_FMT_RGB_888:
	case MPP_FMT_BGR_565:
	case MPP_FMT_RGB_565:
		comp = 1;
		stride[0] = line_stride;
		break;

	default:
		loge("pixel format error, no support pixel_format %d", pixel_format);
		return -1;
	}

	frame->comp_num = comp;

	for(i=0; i<comp; i++) {
		ve_add_dma_buf(frame->frm.mpp_frame.buf.fd[i], &frame->frm.phy_addr[i]);
		frame->frm.mpp_frame.buf.stride[i]	= stride[i];
	}

	return 0;
}

static int rm_dmabuf(struct frame_impl *frame)
{
	int i;
	logd("free frame buffer");

	if (!frame) {
		return -1;
	}

	for(i=0; i<frame->comp_num; i++) {
		ve_rm_dma_buf(frame->frm.mpp_frame.buf.fd[i], frame->frm.phy_addr[i]);
	}

	return 0;
}

static void print_frame_info(struct frame_manager *fm, const char* func_name)
{
	int i;
	struct frame_impl *frm_impl;

	logd("===== %s frame info empty_num: %d, render_num: %d ====", func_name, fm->empty_num, fm->render_num);
	for(i=0; i<fm->frame_count; i++) {
		frm_impl = &fm->frame_node[i];
		logd("i: %d, used_by_decoder: %d, used_by_disp: %d",
			i, frm_impl->used_by_decoder, frm_impl->used_by_display);
	}
}

struct frame_manager *fm_create(struct frame_manager_init_cfg *init_cfg)
{
	struct frame_manager *frm_manager;
	struct frame_impl *frm_impl;
	int ret, i;

	if (!init_cfg || init_cfg->frame_count <= 0 ||
		init_cfg->width <= 0 || init_cfg->height <= 0)
		return NULL;

	logd("create frame manager");
	logi("frame info: width: %d, height: %d, cnt: %d", init_cfg->width, init_cfg->height, init_cfg->frame_count);

	frm_manager = (struct frame_manager *)mpp_alloc(sizeof(struct frame_manager));
	if (!frm_manager)
		return NULL;

	pthread_mutex_init(&frm_manager->lock, NULL);

	frm_manager->frame_count = init_cfg->frame_count;
	frm_manager->empty_num = init_cfg->frame_count;
	frm_manager->render_num = 0;
	if (frm_manager->frame_count > MPP_MAX_FRAME_NUM) {
		logw("frame manager the max frame count %d < %d", MPP_MAX_FRAME_NUM, frm_manager->frame_count);
		frm_manager->frame_count = MPP_MAX_FRAME_NUM;
	}

	if(init_cfg->allocator)
		frm_manager->allocator = init_cfg->allocator;
	else
		frm_manager->allocator = def_open_allocator();

	frm_manager->frame_node = (struct frame_impl *)mpp_alloc(frm_manager->frame_count * sizeof(struct frame_impl));
	if (!frm_manager->frame_node) {
		loge("alloc %d count frame failed!", frm_manager->frame_count);
		mpp_free(frm_manager);
		return NULL;
	}
	memset(frm_manager->frame_node, 0, frm_manager->frame_count * sizeof(struct frame_impl));

	frm_impl = frm_manager->frame_node;
	for (i = 0; i < frm_manager->frame_count; i++) {
		frm_impl->node_id = i;
		frm_impl->used_by_decoder = 0;
		frm_impl->used_by_display = 0;
		frm_impl->ref_count = 0;

		frm_impl->frm.mpp_frame.id = frm_impl->node_id;
		frm_impl->frm.mpp_frame.buf.buf_type = MPP_DMA_BUF_FD;
		frm_impl->frm.mpp_frame.buf.size.width = init_cfg->width;
		frm_impl->frm.mpp_frame.buf.size.height = init_cfg->height;
		frm_impl->frm.mpp_frame.buf.format = init_cfg->pixel_format;

		ret = frm_manager->allocator->ops->alloc_frame_buffer(frm_manager->allocator, &frm_impl->frm.mpp_frame,
			init_cfg->stride, init_cfg->height_align, init_cfg->pixel_format);
		if (ret < 0)
			break;
		add_dmabuf(frm_impl, frm_impl->frm.mpp_frame.buf.stride[0]);

		frm_impl++;
	}

	if (i < frm_manager->frame_count) {
		loge("alloc %d frame buffer failed, only %d frame buffer allocated", frm_manager->frame_count, i);
		for (; i >= 0; i--) {
			rm_dmabuf(&frm_manager->frame_node[i]);
			frm_manager->allocator->ops->free_frame_buffer(frm_manager->allocator,
				&frm_manager->frame_node[i].frm.mpp_frame);
		}
		mpp_free(frm_manager->frame_node);
		mpp_free(frm_manager);
		return NULL;
	}

	mpp_list_init(&frm_manager->empty_list);
	mpp_list_init(&frm_manager->render_list);

	frm_impl = frm_manager->frame_node;
	for (i = 0; i < frm_manager->frame_count; i++) {
		mpp_list_init(&frm_impl->list);
		mpp_list_add_tail(&frm_impl->list, &frm_manager->empty_list);
		frm_impl++;
	}
	logd("create frame manager successful!");

	return frm_manager;
}

int fm_destory(struct frame_manager *fm)
{
	struct frame_impl *frame;
	struct frame_impl *m;
	int i;

	logd("destroy frame manager");

	if (!fm)
		return -1;

	pthread_mutex_lock(&fm->lock);

	if (!mpp_list_empty(&fm->empty_list)) {
		mpp_list_for_each_entry_safe(frame, m, &fm->empty_list, list) {
			mpp_list_del_init(&frame->list);
		}
	}

	if (!mpp_list_empty(&fm->render_list)) {
		mpp_list_for_each_entry_safe(frame, m, &fm->render_list, list) {
			mpp_list_del_init(&frame->list);
		}
	}

	mpp_list_del_init(&fm->empty_list);
	mpp_list_del_init(&fm->render_list);

	pthread_mutex_unlock(&fm->lock);
	pthread_mutex_destroy(&fm->lock);

	frame = fm->frame_node;
	for (i = 0; i < fm->frame_count; i++) {
		rm_dmabuf(frame);
		fm->allocator->ops->free_frame_buffer(fm->allocator,
				&frame->frm.mpp_frame);
		frame++;
	}
	fm->allocator->ops->close_allocator(fm->allocator);


	if (fm->frame_node)
		mpp_free(fm->frame_node);

	if (fm)
		mpp_free(fm);

	return 0;
}

int fm_render_get_frame(struct frame_manager *fm, struct mpp_frame *frame)
{
	struct frame_impl *frm_impl;

	logd("frame manager dequeue render mpp frame");

	if (!fm || !frame)
		return -1;

	pthread_mutex_lock(&fm->lock);

	frm_impl = mpp_list_first_entry_or_null(&fm->render_list, struct frame_impl, list);
	if(!frm_impl) {
		logw("frame manager dequeue mpp frame failed!");
		pthread_mutex_unlock(&fm->lock);
		return -1;
	}

	mpp_list_del_init(&frm_impl->list);
	frm_impl->used_by_display = 1;

	memcpy(frame, &frm_impl->frm.mpp_frame, sizeof(struct mpp_frame));

	fm->render_num--;
	pthread_mutex_unlock(&fm->lock);

	return 0;
}

int fm_render_put_frame(struct frame_manager* fm, struct mpp_frame *frame)
{
	struct frame_impl *frm_impl;
	int id;

	logd("frame manager enqueue empty mpp frame");

	if (!fm || !frame)
		return -1;

	pthread_mutex_lock(&fm->lock);

	id = frame->id;
	if (id < 0 || id >= fm->frame_count) {
		pthread_mutex_unlock(&fm->lock);
		loge("frame manager enqueue empty mpp frame id %d failed!", id);
		return -1;
	}

	frm_impl = &fm->frame_node[id];
	frm_impl->used_by_display = 0;
	frm_impl->displayed = 1;

	frm_impl->ref_count--;
	if (frm_impl->ref_count == 0) {
		mpp_list_del_init(&frm_impl->list);
		mpp_list_add_tail(&frm_impl->list, &fm->empty_list);
		fm->empty_num ++;
		frm_impl->used_by_decoder = 0;
		frm_impl->used_by_display = 0;
	}

	if(frm_impl->ref_count < 0) {
		loge("error: frame ref_count: %d, id: %d", frm_impl->ref_count, frm_impl->node_id);
	}

	pthread_mutex_unlock(&fm->lock);

	return 0;
}

int fm_decoder_put_frame(struct frame_manager* fm, struct frame *frame)
{
	struct frame_impl *frm_impl = (struct frame_impl *)frame;

	if (!fm || !frm_impl)
		return -1;

	logd("fm_decoder_put_frame, ref_count: %d, displayed: %d", frm_impl->ref_count, frm_impl->displayed);

	pthread_mutex_lock(&fm->lock);

	frm_impl->ref_count--;

	//* if ref_count=0 and displayed, add this frame to empty list
	if (frm_impl->ref_count == 0 && frm_impl->displayed) {
		mpp_list_del_init(&frm_impl->list);
		mpp_list_add_tail(&frm_impl->list, &fm->empty_list);
		fm->empty_num ++;
		frm_impl->used_by_decoder = 0;
		frm_impl->used_by_display = 0;
	}

	if(frm_impl->ref_count < 0) {
		loge("error: frame ref_count: %d, id: %d", frm_impl->ref_count, frm_impl->node_id);
	}

	pthread_mutex_unlock(&fm->lock);

	return 0;
}

struct frame *fm_decoder_get_frame(struct frame_manager *fm)
{
	struct frame_impl *frm_impl;

	logd("fm_decoder_get_frame");

	if (!fm)
		return NULL;

	pthread_mutex_lock(&fm->lock);

	frm_impl = mpp_list_first_entry_or_null(&fm->empty_list, struct frame_impl, list);
	if(!frm_impl) {
		pthread_mutex_unlock(&fm->lock);
		return NULL;
	}

	mpp_list_del_init(&frm_impl->list);
	frm_impl->used_by_decoder = 1;
	frm_impl->displayed = 0;
	frm_impl->ref_count++;
	fm->empty_num --;
	pthread_mutex_unlock(&fm->lock);

	// reset flags
	frm_impl->frm.mpp_frame.flags = 0;
	logi("fm_decoder_get_frame end, empty_num: %d", fm->empty_num);
	return (struct frame *)frm_impl;
}

int fm_decoder_frame_to_render(struct frame_manager *fm, struct frame *frame, int display)
{
	struct frame_impl *frm_impl = (struct frame_impl *)frame;

	logd("fm_decoder_frame_to_render, display: %d", display);

	if (!fm || !frm_impl)
		return -1;

	pthread_mutex_lock(&fm->lock);

	if(display) {
		// add this frame to render list
		mpp_list_del_init(&frm_impl->list);
		mpp_list_add_tail(&frm_impl->list, &fm->render_list);
		frm_impl->ref_count++;
		fm->render_num++;
	} else {
		// discard this frame, and add it to empty list if ref_count=0 (not be used by refrence)
		frm_impl->displayed = 1;
		if (frm_impl->ref_count == 0) {
			mpp_list_del_init(&frm_impl->list);
			mpp_list_add_tail(&frm_impl->list, &fm->empty_list);
			fm->empty_num ++;
			frm_impl->used_by_decoder = 0;
			frm_impl->used_by_display = 0;
		}
	}

	if(frm_impl->ref_count < 0) {
		loge("error: frame ref_count: %d, id: %d", frm_impl->ref_count, frm_impl->node_id);
	}

	pthread_mutex_unlock(&fm->lock);

	return 0;
}

struct frame *fm_get_frame_by_id(struct frame_manager *fm, int id)
{
	struct frame_impl *frm_impl;

	logd("frame manager get frame by id");

	if (!fm)
		return NULL;

	if (id < 0 || id >= fm->frame_count) {
		loge("frame manager get frame by id %d failed!", id);
		return NULL;
	}

	frm_impl = &fm->frame_node[id];

	return (struct frame *)frm_impl;
}

int fm_get_empty_frame_num(struct frame_manager *fm)
{
	print_frame_info(fm, __FUNCTION__);
	return fm->empty_num;
}

int fm_get_render_frame_num(struct frame_manager *fm)
{
	return fm->render_num;
}

int fm_reset(struct frame_manager *fm)
{
	if (!mpp_list_empty(&fm->render_list)) {
		struct frame_impl *fame1=NULL,*frame2=NULL;
		mpp_list_for_each_entry_safe(fame1,frame2, &fm->render_list, list) {
			mpp_list_del_init(&fame1->list);
			mpp_list_add_tail(&fame1->list, &fm->empty_list);
		}
	}
	logd("before reset render_num:%d,empty_num:%d,frame_count:%d\n",fm->render_num,fm->empty_num,fm->frame_count);
	fm->empty_num = fm->frame_count;
	fm->render_num = 0;
	return 0;
}