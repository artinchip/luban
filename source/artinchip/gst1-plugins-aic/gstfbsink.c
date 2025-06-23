/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  <qi.xu@artinchip.com>
 *
 *            mpp_frame
 * vedec  ---------------->   fbsink
 *
 * Usage:
 *   gst-launch-1.0 filesrc location=/sdcard/test.mp4 typefind=true ! video/quicktime ! qtdemux ! vedec ! fbsink
 */

#include <stdio.h>
#include <gst/gst.h>
#include <gst/video/video-info.h>
#include <gst/gstbuffer.h>
#include <linux/fb.h>
#include <video/artinchip_fb.h>
#include <sys/ioctl.h>

#include "gstfbsink.h"
#include "gstmppallocator.h"

#define gst_fbsink_parent_class parent_class
G_DEFINE_TYPE (Gstfbsink, gst_fbsink, GST_TYPE_VIDEO_SINK);

GST_DEBUG_CATEGORY_STATIC (gst_fbsink_debug);
#define GST_CAT_DEFAULT gst_fbsink_debug

#define VIDEO_CAPS "{ RGB, BGR, BGRx, xBGR, RGB, RGBx, xRGB, NV12, NV21, I420, YV12 }"

static GstStaticPadTemplate fbsink_template =
GST_STATIC_PAD_TEMPLATE (
	"sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (VIDEO_CAPS))
);

static GstStateChangeReturn
gst_fbsink_change_state (GstElement * element,
    GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	Gstfbsink *fbsink = GST_FBSINK (element);
	GST_DEBUG_OBJECT (fbsink, "%d -> %d",
		GST_STATE_TRANSITION_CURRENT (transition),
		GST_STATE_TRANSITION_NEXT (transition));

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
	// open device

		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		break;
	default:
		break;
	}

	ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		/* Forget everything about the current stream. */
		//gst_framebuffersink_reset (framebuffersink);
		break;
	case GST_STATE_CHANGE_READY_TO_NULL:

		break;
	default:
		break;
	}
	return ret;
}

static int alloc_mpp_buf(Gstfbsink* fbsink, GstVideoInfo *info)
{
	int i;
	struct mpp_buf *buf = NULL;

	for (i=0; i<2; i++) {
		buf = &fbsink->buf[i];
		buf->size.width = info->width;
		buf->size.height = info->height;
		buf->stride[0] = info->width;

		switch (info->finfo->format) {
		case GST_VIDEO_FORMAT_I420:
			buf->format = MPP_FMT_YUV420P;
			buf->stride[1] = buf->stride[2] = buf->stride[0]/2;
			break;
		case GST_VIDEO_FORMAT_YV12:
			buf->format = MPP_FMT_YUV420P;
			buf->stride[1] = buf->stride[2] = buf->stride[0]/2;
			break;
		case GST_VIDEO_FORMAT_NV12:
			buf->format = MPP_FMT_NV12;
			buf->stride[1] = buf->stride[2] = buf->stride[0];
			break;
		case GST_VIDEO_FORMAT_NV21:
			buf->format = MPP_FMT_NV21;
			buf->stride[1] = buf->stride[2] = buf->stride[0];
			break;
		default:
			GST_ERROR_OBJECT(fbsink, "unkown format: %d, %s",
				info->finfo->format, info->finfo->name);
			break;
		}

		mpp_buf_alloc(fbsink->dmabuf_fd, buf);

		printf("buf: %d, fd: %d %d %d\n", i, buf->fd[0], buf->fd[1], buf->fd[2]);
	}

	fbsink->alloc_flag = 1;

	return 0;
}

/*
 * the memory in GstBuffer is struct mpp_frame
 */
static GstFlowReturn gst_fbsink_show_frame (GstBaseSink * bsink, GstBuffer * buffer)
{
	Gstfbsink* fbsink = GST_FBSINK(bsink);
	GstMemory *mem = NULL;
	GstVideoInfo info;
	GstVideoFrame frame;
	struct mpp_frame *mframe;
	int i=0;

	mem = gst_buffer_get_memory(buffer, 0);

	GST_DEBUG_OBJECT(fbsink, " mem ref_count: %d, buffer ref_count: %d",
		GST_MINI_OBJECT_REFCOUNT_VALUE(mem),
		GST_MINI_OBJECT_REFCOUNT_VALUE(buffer));

	if (gst_memory_is_type(mem, GST_MPP_MEMORY_TYPE)) {
		GstMppMemory *mpp_mem = (GstMppMemory*)mem;
		mframe = mpp_mem->frame;

		// hold this buffer
		gst_memory_ref(mem);

		GST_ERROR_OBJECT(fbsink, "render mpp_frame id: %d, w: %d, h: %d, fd: %d %d %d", mframe->id,
			mframe->buf.size.width, mframe->buf.size.height,
			mframe->buf.fd[0], mframe->buf.fd[1], mframe->buf.fd[2]);

		gst_aicfb_render(fbsink->aicfb, &(mframe->buf), mframe->id);

		// ref count of prev memory decrease 1, it will return
		// the previous mpp_frame in mpp_allocator
		if (fbsink->prev_mem != NULL) {
			gst_memory_unref(fbsink->prev_mem);
		}
		fbsink->prev_mem = mem;
	} else {
		int cur_frame_id = fbsink->cur_frame_id;
		GstCaps *caps = gst_pad_get_current_caps (GST_VIDEO_SINK_PAD (fbsink));
		gst_video_info_from_caps (&info, caps);

		gst_video_frame_map (&frame, &info, buffer, GST_MAP_READ);
		GST_DEBUG_OBJECT(fbsink, "frame width: %d, height: %d, format: %d, name: %s",
			info.width, info.height, info.finfo->format, info.finfo->name);


		if(!fbsink->alloc_flag) {
			alloc_mpp_buf(fbsink, &info);
		}

		int datasize[3] = {info.width * info.height, info.width * info.height/4, info.width * info.height/4};
		int comp = 1;
		if(info.finfo->format == GST_VIDEO_FORMAT_I420 || info.finfo->format == GST_VIDEO_FORMAT_YV12) {
			comp = 3;
		} else if(info.finfo->format == GST_VIDEO_FORMAT_NV12 || info.finfo->format == GST_VIDEO_FORMAT_NV21) {
			comp = 2;
			datasize[1] = datasize[0]/2;
		}

		for (i=0; i<comp; i++) {
			unsigned char* vaddr = dmabuf_mmap(fbsink->buf[cur_frame_id].fd[i], datasize[i]);
			memcpy(vaddr, GST_VIDEO_FRAME_COMP_DATA (&frame, i), datasize[i]);
			dmabuf_sync(fbsink->buf[cur_frame_id].fd[i], CACHE_CLEAN);
			dmabuf_munmap(vaddr, datasize[i]);
		}

		gst_aicfb_render(fbsink->aicfb, &fbsink->buf[cur_frame_id], cur_frame_id);
		fbsink->cur_frame_id = (cur_frame_id == 0? 1: 0);
	}

	gst_memory_unref(mem);

	return 0;
}

static gboolean gst_framebuffersink_start (GstBaseSink *sink)
{
	Gstfbsink *fbsink = GST_FBSINK(sink);
	fbsink->aicfb = gst_aicfb_open();
	fbsink->dmabuf_fd = dmabuf_device_open();

	return TRUE;
}

static gboolean gst_framebuffersink_stop (GstBaseSink *sink)
{
	Gstfbsink *fbsink = GST_FBSINK(sink);

	if (fbsink->prev_mem)
		gst_memory_unref(fbsink->prev_mem);

	if(fbsink->aicfb)
		gst_aicfb_close(fbsink->aicfb);
	if(fbsink->dmabuf_fd)
		dmabuf_device_close(fbsink->dmabuf_fd);

	return TRUE;
}

static void gst_fbsink_finalize (Gstfbsink * fbsink)
{
	if(fbsink->alloc_flag) {
		mpp_buf_free(&fbsink->buf[0]);
		mpp_buf_free(&fbsink->buf[1]);
	}
	G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (fbsink));
}

/* initialize the fbsink's class */
static void gst_fbsink_class_init (GstfbsinkClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *element_class;
	GstBaseSinkClass *basesink_class;

	gobject_class = (GObjectClass *) klass;
	element_class = (GstElementClass *) klass;
	basesink_class = GST_BASE_SINK_CLASS (klass);

	gobject_class->finalize = (GObjectFinalizeFunc) gst_fbsink_finalize;
	//gobject_class->set_property = gst_fbsink_set_property;
	//gobject_class->get_property = gst_fbsink_get_property;

	gst_element_class_add_pad_template (element_class,
		gst_static_pad_template_get (&fbsink_template));

	element_class->change_state = GST_DEBUG_FUNCPTR (
		gst_fbsink_change_state);
	basesink_class->render = GST_DEBUG_FUNCPTR (gst_fbsink_show_frame);
	basesink_class->start = GST_DEBUG_FUNCPTR (gst_framebuffersink_start);
	basesink_class->stop = GST_DEBUG_FUNCPTR (gst_framebuffersink_stop);

	GST_DEBUG_CATEGORY_INIT (gst_fbsink_debug, "fbsink", 0, "ArtInChip fbsink");

	gst_element_class_set_static_metadata (element_class,
		"ArtInChip FrameBuffer Sink", "Sink/Video",
		"Displays frames on ArtInChip DE device",
		"<qi.xu@artinchip.com>");
}

/* initialize the new element
 * initialize instance structure
 */
static void gst_fbsink_init (Gstfbsink *fbsink)
{
	fbsink->prev_mem = NULL;
}
