/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  <qi.xu@artinchip.com>
 */

#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "mpp_decoder.h"
#include "gstvedec.h"

#define  gst_ve_dec_parent_class parent_class
G_DEFINE_TYPE (GstVeDec, gst_ve_dec, GST_TYPE_VIDEO_DECODER);

#define GST_VE_DEC_TASK_STARTED(decoder) \
    (gst_pad_get_task_state ((decoder)->srcpad) == GST_TASK_STARTED)

struct gst_mpp_format {
	GstVideoFormat gst_format;
	enum mpp_pixel_format mpp_format;
};

struct gst_mpp_format gst_mpp_formats[] = {
	{GST_VIDEO_FORMAT_I420,  MPP_FMT_YUV420P},
	{GST_VIDEO_FORMAT_NV12,  MPP_FMT_NV12},
	{GST_VIDEO_FORMAT_NV21,  MPP_FMT_NV21},
	{GST_VIDEO_FORMAT_RGB,   MPP_FMT_RGB_888},
	{GST_VIDEO_FORMAT_BGR,   MPP_FMT_BGR_888},
	{GST_VIDEO_FORMAT_RGBA,  MPP_FMT_RGBA_8888},
	{GST_VIDEO_FORMAT_BGRA,  MPP_FMT_BGRA_8888},
	{GST_VIDEO_FORMAT_ARGB,  MPP_FMT_ARGB_8888},
	{GST_VIDEO_FORMAT_ABGR,  MPP_FMT_ABGR_8888},
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

#define GET_GST_FORMAT(format) ({ \
	struct gst_mpp_format *_tmp; \
	for (guint i = 0; i < ARRAY_SIZE (gst_mpp_formats); i++) { \
		_tmp = &gst_mpp_formats[i]; \
		if (_tmp->mpp_format == format) break;\
	}; _tmp->gst_format; \
})

static gboolean gst_ve_dec_open (GstVideoDecoder * bdec)
{
	GST_DEBUG_OBJECT(bdec, "gst_ve_dec_open");
	return TRUE;
}

static gboolean gst_ve_dec_close (GstVideoDecoder * bdec)
{
	GST_DEBUG_OBJECT(bdec, "gst_ve_dec_close");
	return TRUE;
}

static gboolean gst_ve_dec_start (GstVideoDecoder * bdec)
{
	GstVeDec *dec = GST_VE_DEC(bdec);
	dec->pts2frame = g_hash_table_new(NULL, NULL);
	return TRUE;
}

static gboolean gst_ve_dec_stop (GstVideoDecoder * bdec)
{
	GstVeDec *dec = GST_VE_DEC(bdec);
	if(dec->pts2frame) {
		g_hash_table_destroy(dec->pts2frame);
		dec->pts2frame = NULL;
	}
	dec->stop_flag = 1;

	if (dec->input_state) {
		gst_video_codec_state_unref (dec->input_state);
		dec->input_state = NULL;
	}

	gst_pad_stop_task (bdec->srcpad);
	gst_task_stop(dec->dec_task);

	return TRUE;
}

static void gst_decode_loop (GstVeDec * self)
{
	int ret;
	if (self->stop_flag)
		return;

	ret = mpp_decoder_decode(self->mpp_dec);
	if(ret < 0) {
		GST_ERROR_OBJECT(self, "decode error");
		self->dec_task_ret = GST_FLOW_ERROR;
	} else if(ret > 0) {
		g_usleep(1000);
	}
}

static gboolean update_video_info(GstVideoDecoder * decoder, GstVideoFormat format,
    guint width, guint height)
{
	GstVeDec *self = GST_VE_DEC (decoder);
	GstVideoInfo *info = &self->info;

	GstVideoCodecState *output_state;
	output_state = gst_video_decoder_set_output_state (decoder, format,
		GST_ROUND_UP_2 (width), GST_ROUND_UP_2 (height), self->input_state);

	output_state->caps = gst_video_info_to_caps (&output_state->info);
	*info = output_state->info;
	gst_video_codec_state_unref (output_state);

	if (!gst_video_decoder_negotiate (decoder))
		return FALSE;

	return TRUE;
}

static void gst_render_loop (GstVideoDecoder * decoder)
{
	GstVeDec *self = GST_VE_DEC (decoder);
	GstVideoCodecFrame *out_frame;
	struct mpp_frame frame;
	int dec_ret = 0;

	if (self->stop_flag) {
		GST_DEBUG_OBJECT(self, "eos");
		self->task_ret = GST_FLOW_EOS;
		goto out;
	}

	dec_ret = mpp_decoder_get_frame(self->mpp_dec, &frame);
	if(dec_ret == DEC_NO_RENDER_FRAME || dec_ret == DEC_ERR_FM_NOT_CREATE
		|| dec_ret == DEC_NO_EMPTY_FRAME) {
			g_usleep(1000);
		return;
	} else if(dec_ret) {
		GST_ERROR_OBJECT(self, "mpp_dec_get_frame error, ret: %x", dec_ret);
		return;
	}

	if (self->width != frame.buf.size.width || self->height != frame.buf.size.height) {
		GstVideoFormat gst_format = GET_GST_FORMAT(frame.buf.format);
		update_video_info(decoder, gst_format, frame.buf.size.width, frame.buf.size.height);
		self->width = frame.buf.size.width;
		self->height = frame.buf.size.height;
	}

	out_frame = g_hash_table_lookup(self->pts2frame, (gpointer)frame.pts);
	if(out_frame == NULL) {
		GST_ERROR_OBJECT(self, "cannot find out_frame, maybe error");
	}
	g_hash_table_remove(self->pts2frame, (gpointer)frame.pts);

	//if (out_frame->output_buffer == NULL)
	out_frame->output_buffer = gst_buffer_new_allocate((GstAllocator*)self->allocator, sizeof(struct mpp_frame), NULL);

	out_frame->output_buffer->offset = 0;
	out_frame->output_buffer->pts = frame.pts;
	out_frame->output_buffer->dts = out_frame->input_buffer->dts;
	out_frame->output_buffer->duration = out_frame->duration;
	GST_ERROR_OBJECT(self, "frame.pts: %lld, %ld, id: %d, duration: %ld",
		frame.pts, out_frame->pts, frame.id, out_frame->duration);

	// ref_cnt of mpp_mem will increase 1, if we call gst_buffer_get_memory
	GstMppMemory *mpp_mem = (GstMppMemory *)gst_buffer_get_memory(out_frame->output_buffer, 0);
	memcpy(mpp_mem->frame, &frame, sizeof(struct mpp_frame));
	gst_memory_unref((GstMemory*)mpp_mem);

	gst_video_codec_frame_ref(out_frame);

	gst_video_decoder_finish_frame(decoder, out_frame);

	GST_ERROR_OBJECT(self, " mem ref_count: %d, output_buffer ref_count: %d, id: %d",
		GST_MINI_OBJECT_REFCOUNT_VALUE(mpp_mem),
		GST_MINI_OBJECT_REFCOUNT_VALUE(out_frame->output_buffer),
		frame.id);

	GST_DEBUG_OBJECT(self, "video dec ts: %" GST_TIME_FORMAT ", dur:%" GST_TIME_FORMAT" \n",
		GST_TIME_ARGS (GST_BUFFER_PTS (out_frame->input_buffer)),
		GST_TIME_ARGS (GST_BUFFER_DURATION (out_frame->input_buffer)));

	GST_DEBUG_OBJECT(self, "out video dec ts: %" GST_TIME_FORMAT ", dur:%" GST_TIME_FORMAT" \n",
		GST_TIME_ARGS (GST_BUFFER_PTS (out_frame->output_buffer)),
		GST_TIME_ARGS (GST_BUFFER_DURATION (out_frame->output_buffer)));

out:
	if (self->task_ret != GST_FLOW_OK) {
		gst_pad_pause_task(decoder->srcpad);
	}
}

static GstFlowReturn gst_ve_dec_handle_frame (GstVideoDecoder * bdec, GstVideoCodecFrame * in_frame)
{
	GstVeDec *dec = GST_VE_DEC(bdec);
	GstMapInfo input_minfo;
	struct mpp_packet packet;
	int timeout_us = 10000000; // 10s
	int wait_time_us = 10000;
	GstFlowReturn ret = GST_FLOW_OK;
	memset(&packet, 0, sizeof(struct mpp_packet));

	if (!GST_VE_DEC_TASK_STARTED(bdec)) {
		gst_pad_start_task (bdec->srcpad,
       			(GstTaskFunction) gst_render_loop, bdec, NULL);

		gst_task_start(dec->dec_task);
	}

	if (in_frame == NULL) {
		GST_ERROR_OBJECT(dec, "frame is NULL");
		return GST_FLOW_ERROR;
	}
	gst_buffer_map (in_frame->input_buffer, &input_minfo, GST_MAP_READ);

	//printf("===> 1. handle frame start ref: %d, sys_frm_num: %d, input_minfo.size: %d\n",
	//	in_frame->ref_count, in_frame->system_frame_number, input_minfo.size);

	GST_VIDEO_DECODER_STREAM_UNLOCK (bdec);
	while (1) {
		int ret = mpp_decoder_get_packet(dec->mpp_dec, &packet, input_minfo.size);
		if (ret == 0) {
			break;
		}
		g_usleep(wait_time_us);
		timeout_us -= wait_time_us;

		if (dec->stop_flag) {
			GST_DEBUG_OBJECT(dec, "stop");
			ret = GST_FLOW_ERROR;
			goto done;
		}
	}

	packet.size = input_minfo.size;
	memcpy(packet.data, input_minfo.data, input_minfo.size);
	packet.pts = in_frame->pts;
	mpp_decoder_put_packet(dec->mpp_dec, &packet);

	g_hash_table_insert(dec->pts2frame, (gpointer)in_frame->pts, (gpointer)in_frame);

	GST_VIDEO_DECODER_STREAM_LOCK (bdec);


done:
	gst_buffer_unmap (in_frame->input_buffer, &input_minfo);
	gst_video_codec_frame_unref (in_frame);

	//printf("===> 1.1 handle frame ref: %d, sys_frm_num: %d\n",
	//	in_frame->ref_count, in_frame->system_frame_number);

	return ret == GST_FLOW_ERROR ? GST_FLOW_ERROR : dec->task_ret;
}

static gboolean gst_ve_dec_set_format (GstVideoDecoder * bdec, GstVideoCodecState * state)
{
	GstVeDec *dec = GST_VE_DEC(bdec);

	const gchar *mimetype = gst_structure_get_name (gst_caps_get_structure (state->caps, 0));
	if(!strcmp(mimetype, "video/x-h264")) {
		dec->dec_type = MPP_CODEC_VIDEO_DECODER_H264;
	} else if(!strcmp(mimetype, "image/jpeg")) {
		dec->dec_type = MPP_CODEC_VIDEO_DECODER_MJPEG;
	} else if(!strcmp(mimetype, "image/png")) {
		dec->dec_type = MPP_CODEC_VIDEO_DECODER_PNG;
	}

	if(dec->mpp_dec) {
		mpp_decoder_destory(dec->mpp_dec);
		dec->mpp_dec = NULL;
	}
	dec->mpp_dec = mpp_decoder_create(dec->dec_type);
	dec->allocator = gst_mpp_allocator_new(dec->mpp_dec);

	struct decode_config config;
	config.bitstream_buffer_size = 1024*1024;
	config.extra_frame_num = 2;
	config.packet_count = 10;

	if(dec->dec_type == MPP_CODEC_VIDEO_DECODER_PNG)
		config.pix_fmt = MPP_FMT_RGBA_8888;
	else
		config.pix_fmt = MPP_FMT_YUV420P;

	// init mpp_decoder
	mpp_decoder_init(dec->mpp_dec, &config);

	// put extra data
	GstMapInfo minfo;
	// there is not extradata if it is ts file
	if (state->codec_data) {
		gst_buffer_map (state->codec_data, &minfo, GST_MAP_READ);

		struct mpp_packet packet;
		memset(&packet, 0, sizeof(struct mpp_packet));
		mpp_decoder_get_packet(dec->mpp_dec, &packet, minfo.size);
		memcpy(packet.data, minfo.data, minfo.size);
		packet.size = minfo.size;
		packet.flag = PACKET_FLAG_EXTRA_DATA;
		mpp_decoder_put_packet(dec->mpp_dec, &packet);

		gst_buffer_unmap (state->codec_data, &minfo);

		// decode extradata
		if(mpp_decoder_decode(dec->mpp_dec) < 0) {
			GST_ERROR_OBJECT(dec, "decode extradata failed");
		}

		dec->input_state = gst_video_codec_state_ref (state);
	}

	return TRUE;
}

static GstFlowReturn gst_ve_dec_finish (GstVideoDecoder * bdec)
{
	GstVeDec *dec = GST_VE_DEC(bdec);
	dec->stop_flag = 1;
	return TRUE;
}

static gboolean gst_ve_dec_decide_allocation (GstVideoDecoder * bdec, GstQuery * query)
{
	GstVeDec *decoder = GST_VE_DEC(bdec);
	GstCaps *outcaps = NULL;
	GstBufferPool *pool = NULL;
	guint size, min, max;
	GstAllocator *allocator = NULL;
	GstAllocationParams params;
	GstStructure *config;
	gboolean update_pool, update_allocator;
	GstVideoInfo vinfo;

	gst_query_parse_allocation (query, &outcaps, NULL);
	gst_video_info_init (&vinfo);
	if (outcaps)
		gst_video_info_from_caps (&vinfo, outcaps);

	/* we got configuration from our peer or the decide_allocation method,
	* parse them */
	if (gst_query_get_n_allocation_params (query) > 0) {
		/* try the allocator */
		gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
		update_allocator = TRUE;
	} else {
		allocator = NULL;
		gst_allocation_params_init (&params);
		update_allocator = FALSE;
	}

	if (gst_query_get_n_allocation_pools (query) > 0) {
		gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
		size = MAX (size, vinfo.size);
		update_pool = TRUE;
	} else {
		pool = NULL;
		size = vinfo.size;
		min = max = 0;

		update_pool = FALSE;
	}

	if (pool == NULL) {
		/* no pool, we can make our own */
		GST_DEBUG_OBJECT (decoder, "no pool, making new pool");
		pool = gst_video_buffer_pool_new ();
	}

	/* now configure */
	config = gst_buffer_pool_get_config (pool);
	gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
	gst_buffer_pool_config_set_allocator (config, allocator, &params);

	GST_DEBUG_OBJECT (decoder,
		"setting config %" GST_PTR_FORMAT " in pool %" GST_PTR_FORMAT, config,
		pool);
	if (!gst_buffer_pool_set_config (pool, config)) {
		config = gst_buffer_pool_get_config (pool);

		/* If change are not acceptable, fallback to generic pool */
		if (!gst_buffer_pool_config_validate_params (config, outcaps, size, min,
			max)) {
			GST_DEBUG_OBJECT (decoder, "unsupported pool, making new pool");

			gst_object_unref (pool);
			pool = gst_video_buffer_pool_new ();
			gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
			gst_buffer_pool_config_set_allocator (config, allocator, &params);
		}

		if (!gst_buffer_pool_set_config (pool, config))
			goto config_failed;
	}

	if (update_allocator)
		gst_query_set_nth_allocation_param (query, 0, allocator, &params);
	else
		gst_query_add_allocation_param (query, allocator, &params);
	if (allocator)
		gst_object_unref (allocator);

	if (update_pool)
		gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
	else
		gst_query_add_allocation_pool (query, pool, size, min, max);

	if (pool)
		gst_object_unref (pool);

	return TRUE;

config_failed:
	if (allocator)
		gst_object_unref (allocator);
	if (pool)
		gst_object_unref (pool);
	GST_ELEMENT_ERROR (decoder, RESOURCE, SETTINGS,
		("Failed to configure the buffer pool"),
		("Configuration is most likely invalid, please report this issue."));
	return FALSE;
}

static gboolean gst_ve_dec_reset (GstVideoDecoder * bdec, gboolean hard)
{
	return TRUE;
}

static GstCaps *get_sink_caps (void)
{
	static GstCaps *caps = NULL;

	caps = gst_caps_from_string ("video/x-h264");
	GstCaps *newcaps = gst_caps_from_string ("image/jpeg");
	gst_caps_append (caps, newcaps);
	newcaps = gst_caps_from_string ("image/png");
	gst_caps_append (caps, newcaps);

	return gst_caps_ref (caps);
}

static GstCaps *get_src_caps (void)
{
	static GstCaps *caps = NULL;

	if (caps == NULL) {
		caps = gst_caps_from_string (GST_VIDEO_CAPS_MAKE ("{ NV12, I420, YV12, NV16, Y444}"));
	}

	return gst_caps_ref (caps);
}

static void gst_ve_dec_finalize (GObject * object)
{
	GstVeDec *self = (GstVeDec*)object;
	gst_task_join(self->dec_task);
	gst_object_unref(self->dec_task);
	g_rec_mutex_clear (&self->lock);
	if (self->allocator)
		gst_object_unref (self->allocator);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean gst_ve_dec_sink_event (GstVideoDecoder * decoder,
    GstEvent * event)
{
	int ret;
	GstVeDec *self = (GstVeDec*)decoder;

	switch (GST_EVENT_TYPE (event)) {
	case GST_EVENT_CUSTOM_UPSTREAM: {
		GST_ERROR_OBJECT(self, "======> sink event\n");
	}

	default:
		break;
	}

	ret = GST_VIDEO_DECODER_CLASS (parent_class)->sink_event (decoder, event);

	return ret;
}

static gboolean gst_ve_dec_src_event (GstVideoDecoder * decoder,
    GstEvent * event)
{
	int ret;
	GstVeDec *self = (GstVeDec*)decoder;

	switch (GST_EVENT_TYPE (event)) {
	case GST_EVENT_CUSTOM_UPSTREAM: {
		const GstStructure *str = gst_event_get_structure (event);

		if (gst_structure_has_name(str, "return-frame")) {
			int val;
			gst_structure_get_int(str, "id", &val);
			GST_DEBUG_OBJECT(self, "return frame.id: %d\n", val);
		}
	}

	default:
		break;
	}

	ret = GST_VIDEO_DECODER_CLASS (parent_class)->src_event (decoder, event);

	return ret;
}

static void gst_ve_dec_class_init (GstVeDecClass * klass)
{
	GstElementClass *element_class;
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GstVideoDecoderClass *vdec_class;

	element_class = GST_ELEMENT_CLASS (klass);
	vdec_class = GST_VIDEO_DECODER_CLASS (klass);

	gst_element_class_add_pad_template (element_class,
		gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, get_sink_caps()));
	gst_element_class_add_pad_template (element_class,
		gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, get_src_caps()));

	// we must set metadata below, or plugin init failed
	gst_element_class_set_static_metadata (element_class,
		"VE-based video decoder", "Codec/Decoder/Video",
		"Decode compressed video to raw data",
		"<qi.xu@artinchip.com>");

	gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_ve_dec_finalize);

	vdec_class->src_event = GST_DEBUG_FUNCPTR(gst_ve_dec_src_event);
	vdec_class->sink_event = GST_DEBUG_FUNCPTR(gst_ve_dec_sink_event);
	vdec_class->open = GST_DEBUG_FUNCPTR (gst_ve_dec_open);
	vdec_class->close = GST_DEBUG_FUNCPTR (gst_ve_dec_close);
	vdec_class->start = GST_DEBUG_FUNCPTR (gst_ve_dec_start);
	vdec_class->stop = GST_DEBUG_FUNCPTR (gst_ve_dec_stop);
	vdec_class->set_format = GST_DEBUG_FUNCPTR (gst_ve_dec_set_format);
	vdec_class->handle_frame = GST_DEBUG_FUNCPTR (gst_ve_dec_handle_frame);
	vdec_class->finish = GST_DEBUG_FUNCPTR (gst_ve_dec_finish);
	vdec_class->decide_allocation = GST_DEBUG_FUNCPTR (gst_ve_dec_decide_allocation);
	vdec_class->reset = GST_DEBUG_FUNCPTR (gst_ve_dec_reset);

	GST_DEBUG_OBJECT(vdec_class, "gst_ve_dec_class_init end");
}

static void gst_ve_dec_init (GstVeDec * self)
{
	/* As Ve can support stream mode. need call parser before decode */
	gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);

	self->task_ret = GST_FLOW_OK;
	self->dec_task = gst_task_new((GstTaskFunction) gst_decode_loop, self, NULL);

	g_rec_mutex_init (&self->lock);
	gst_task_set_lock (self->dec_task, &self->lock);
}
