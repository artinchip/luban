/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc: mpp_decoder interface
*/

#include "mpp_codec.h"
#include "frame_allocator.h"
#include "mpp_log.h"

extern struct mpp_decoder* create_jpeg_decoder();
extern struct mpp_decoder* create_png_decoder();
extern struct mpp_decoder* create_h264_decoder();

struct mpp_decoder* mpp_decoder_create(enum mpp_codec_type type)
{
	if(type == MPP_CODEC_VIDEO_DECODER_MJPEG)
		return create_jpeg_decoder();
	else if(type == MPP_CODEC_VIDEO_DECODER_H264)
		return create_h264_decoder();
	else if(type == MPP_CODEC_VIDEO_DECODER_PNG)
		return create_png_decoder();
	return NULL;
}

void mpp_decoder_destory(struct mpp_decoder* decoder)
{
	decoder->ops->destory(decoder);
}

int mpp_decoder_init(struct mpp_decoder *decoder, struct decode_config *config)
{
	return decoder->ops->init(decoder, config);
}

int mpp_decoder_decode(struct mpp_decoder* decoder)
{
	if(decoder->pm && decoder->fm) {
		logd("render frame number: %d, empty frame number: %d", fm_get_render_frame_num(decoder->fm),
		  fm_get_empty_frame_num(decoder->fm));
	}

	if(decoder->pm && pm_get_ready_packet_num(decoder->pm) == 0)
		return DEC_NO_READY_PACKET;

	return decoder->ops->decode(decoder);
}

int mpp_decoder_control(struct mpp_decoder* decoder, int cmd, void *param)
{
	struct mpp_scale_ratio* ratio = NULL;
	struct mpp_dec_crop_info* info = NULL;
	struct mpp_dec_output_pos* pos = NULL;
	switch (cmd) {
	case MPP_DEC_INIT_CMD_SET_EXT_FRAME_ALLOCATOR:
		decoder->allocator = (struct frame_allocator*)param;
		return 0;
	case MPP_DEC_INIT_CMD_SET_ROT_FLIP_FLAG:
		decoder->rotmir_flag = *((int*)param);
		logi("decoder->rotmir_flag: %d", decoder->rotmir_flag);
		return 0;
	case MPP_DEC_INIT_CMD_SET_SCALE:
		ratio = (struct mpp_scale_ratio*)param;
		decoder->ver_scale = ratio->ver_scale;
		decoder->hor_scale = ratio->hor_scale;
		return 0;
	case MPP_DEC_INIT_CMD_SET_CROP_INFO:
		info = (struct mpp_dec_crop_info*)param;
		decoder->crop_en = 1;
		decoder->crop_x = info->crop_x;
		decoder->crop_y = info->crop_y;
		decoder->crop_width = info->crop_width;
		decoder->crop_height = info->crop_height;
		return 0;
	case MPP_DEC_INIT_CMD_SET_OUTPUT_POS:
		pos = (struct mpp_dec_output_pos*)param;
		decoder->output_x = pos->output_pos_x;
		decoder->output_y = pos->output_pos_y;
		return 0;
	default:
		break;
	}

	return decoder->ops->control(decoder, cmd, param);
}

int mpp_decoder_reset(struct mpp_decoder* decoder)
{
	return decoder->ops->reset(decoder);
}

int mpp_decoder_get_packet(struct mpp_decoder* decoder, struct mpp_packet* packet, int size)
{
	if(decoder == NULL || packet == NULL)
		return DEC_ERR_NULL_PTR;
	if(pm_get_empty_packet_num(decoder->pm) == 0)
		return DEC_NO_EMPTY_PACKET;

	return pm_dequeue_empty_packet(decoder->pm, packet, size);
}

int mpp_decoder_put_packet(struct mpp_decoder* decoder, struct mpp_packet* packet)
{
	if(decoder == NULL || packet == NULL)
		return DEC_ERR_NULL_PTR;

	return pm_enqueue_ready_packet(decoder->pm, packet);
}

int mpp_decoder_get_frame(struct mpp_decoder* decoder, struct mpp_frame* frame)
{
	if(decoder == NULL || frame == NULL)
		return DEC_ERR_NULL_PTR;

	if(decoder->fm == NULL) {
		//logw("frame manager not create now, wait");
		return DEC_ERR_FM_NOT_CREATE;
	}

	if(fm_get_render_frame_num(decoder->fm) == 0)
		return DEC_NO_RENDER_FRAME;

	return fm_render_get_frame(decoder->fm, frame);
}

int mpp_decoder_put_frame(struct mpp_decoder* decoder, struct mpp_frame* frame)
{
	if(decoder == NULL || frame == NULL)
		return DEC_ERR_NULL_PTR;
	return fm_render_put_frame(decoder->fm, frame);
}
