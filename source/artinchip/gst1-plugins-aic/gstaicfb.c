/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 *
 * Authors:  <qi.xu@artinchip.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/fb.h>
#include <video/artinchip_fb.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <gst/gst.h>

#include "gstaicfb.h"

#define FRAME_BUF_NUM		(18)
struct frame_info {
	int fd[3];		// dma-buf fd
	int fd_num;		// number of dma-buf
	int used;		// if the dma-buf of this frame add to de drive
};

struct gst_aicfb {
	int fd;
	int screen_width;
	int screen_height;
	struct frame_info infos[FRAME_BUF_NUM];
};

struct gst_aicfb* gst_aicfb_open()
{
	struct mpp_size s = {0};
	struct gst_aicfb* aicfb = (struct gst_aicfb*)malloc(sizeof(struct gst_aicfb));
	if (aicfb == NULL) {
		GST_ERROR_OBJECT(NULL, "malloc failed!");
		goto error;
	}
	memset(aicfb, 0, sizeof(struct gst_aicfb));

	aicfb->fd = open("/dev/fb0", O_RDWR);
	if (aicfb->fd < 0) {
		GST_ERROR_OBJECT(NULL, "open fb0 failed!");
		goto error;
	}

	if (ioctl(aicfb->fd, AICFB_GET_SCREEN_SIZE, &s) == -1) {
		GST_ERROR_OBJECT(NULL, "read fixed information failed!");
		goto error;
	}

	aicfb->screen_width = s.width;
	aicfb->screen_height = s.height;

	return aicfb;

error:
	if (aicfb) {
		if (aicfb->fd)
			close(aicfb->fd);
		free(aicfb);
	}
	return NULL;
}

void gst_aicfb_close(struct gst_aicfb* aicfb)
{
	int i, j;
	struct aicfb_layer_data layer = {0};

	if (aicfb == NULL)
		return;

	// disable layer
	layer.enable = 0;
	if(ioctl(aicfb->fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		GST_ERROR_OBJECT(NULL, "fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	// remove dmabuf to de driver
	for (i=0; i<FRAME_BUF_NUM; i++) {
		if (aicfb->infos[i].used == 0)
			continue;
		for (j=0; j<aicfb->infos[i].fd_num; j++) {
			if (ioctl(aicfb->fd, AICFB_RM_DMABUF, &aicfb->infos[i].fd[j]) < 0)
				GST_ERROR_OBJECT(NULL, "fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
		}
	}

	close(aicfb->fd);

	free(aicfb);
}


int gst_aicfb_render(struct gst_aicfb* aicfb, struct mpp_buf* buf, int buf_id)
{
	int ret = 0;
	int i;
	int dmabuf_num = 0;
	struct aicfb_alpha_config alpha = {0};
	struct aicfb_layer_data layer = {0};
	struct dma_buf_info dmabuf_fd[3];

	alpha.layer_id = 1;
	alpha.enable = 1;
	alpha.mode = 1;
	alpha.value = 0;
	ret = ioctl(aicfb->fd, AICFB_UPDATE_ALPHA_CONFIG, &alpha);
	if (ret < 0)
		GST_ERROR_OBJECT(NULL, "fb ioctl() AICFB_UPDATE_ALPHA_CONFIG failed!");

	layer.layer_id = AICFB_LAYER_TYPE_VIDEO;
	layer.enable = 1;
	layer.scale_size.width = aicfb->screen_width;
	layer.scale_size.height= aicfb->screen_height;
	layer.pos.x = 0;
	layer.pos.y = 0;
	memcpy(&layer.buf, buf, sizeof(struct mpp_buf));

	if (buf->format == MPP_FMT_ARGB_8888) {
		dmabuf_num = 1;
	} else if (buf->format == MPP_FMT_RGBA_8888) {
		dmabuf_num = 1;
	} else if (buf->format == MPP_FMT_RGB_888) {
		dmabuf_num = 1;
	} else if (buf->format == MPP_FMT_YUV420P) {
		dmabuf_num = 3;
	} else if (buf->format == MPP_FMT_YUV444P) {
		dmabuf_num = 3;
	} else if (buf->format == MPP_FMT_YUV422P) {
		dmabuf_num = 3;
	} else if (buf->format == MPP_FMT_YUV400) {
		dmabuf_num = 1;
	} else {
		GST_ERROR_OBJECT(NULL, "no support picture foramt %d, default argb8888", buf->format);
	}

	aicfb->infos[buf_id].fd_num = dmabuf_num;

	// add dmabuf to de driver
	if (0 == aicfb->infos[buf_id].used) {
		for(i=0; i<dmabuf_num; i++) {
			aicfb->infos[buf_id].fd[i] = buf->fd[i];
			dmabuf_fd[i].fd = buf->fd[i];
			if (ioctl(aicfb->fd, AICFB_ADD_DMABUF, &dmabuf_fd[i]) < 0)
				GST_ERROR_OBJECT(NULL, "fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");
		}
		aicfb->infos[buf_id].used = 1;
	}

	// update layer config (it is async interface)
	if (ioctl(aicfb->fd, AICFB_UPDATE_LAYER_CONFIG, &layer) < 0)
		GST_ERROR_OBJECT(NULL, "fb ioctl() AICFB_UPDATE_LAYER_CONFIG failed!");

	// wait vsync (wait layer config)
	ioctl(aicfb->fd, AICFB_WAIT_FOR_VSYNC, NULL);

	return 0;
}
