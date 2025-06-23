/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_audio_decoder interface
 */


#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>

#include "mpp_dec_type.h"
#include "mpp_mem.h"
#include "mpp_list.h"
#include "mpp_log.h"

#include "audio_decoder.h"
#include "mad.h"

#define MAD_BUFFER_LEEN (4*1024)

struct mp3_audio_decoder {
	struct aic_audio_decoder decoder;
	struct mpp_packet* curr_packet;
	struct mpp_packet curr_packet_info;
	unsigned char *mad_buffer;
	int mad_buffer_len;
	int channels;
	int sample_rate;
	int bits_per_sample;
	uint32_t frame_id;
	int frame_count;
	struct mad_stream stream;
	struct mad_frame frame;
	struct mad_synth synth;
	int final_frame;
};

int __mp3_decode_init(struct aic_audio_decoder *decoder, struct aic_audio_decode_config *config)
{
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;
	mp3_decoder->mad_buffer = mpp_alloc(MAD_BUFFER_LEEN);
	mp3_decoder->mad_buffer_len = MAD_BUFFER_LEEN;
	mp3_decoder->decoder.pm = audio_pm_create(config);
	mp3_decoder->frame_count = config->frame_count;
	mp3_decoder->frame_id = 0;
	mad_stream_init(&mp3_decoder->stream);
	mad_frame_init(&mp3_decoder->frame);
	mad_synth_init(&mp3_decoder->synth);
	return 0;
}

int __mp3_decode_destroy(struct aic_audio_decoder *decoder)
{
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;
	if (!mp3_decoder) {
		return -1;
	}
	if (mp3_decoder->mad_buffer) {
		mpp_free(mp3_decoder->mad_buffer);
		mp3_decoder->mad_buffer = NULL;
	}
	if (mp3_decoder->decoder.pm) {
		audio_pm_destroy(mp3_decoder->decoder.pm);
		mp3_decoder->decoder.pm = NULL;
	}
	if (mp3_decoder->decoder.fm) {
		audio_fm_destroy(mp3_decoder->decoder.fm);
		mp3_decoder->decoder.fm = NULL;
	}
	mad_frame_finish(&mp3_decoder->frame);
	mad_stream_finish(&mp3_decoder->stream);
	mad_synth_finish(&mp3_decoder->header);
	mpp_free(mp3_decoder);
	return 0;

}

static inline signed int scale(mad_fixed_t sample)
{
	/* round */
	sample += (1L << (MAD_F_FRACBITS - 16));

	/* clip */
	if (sample >= MAD_F_ONE)
		sample = MAD_F_ONE - 1;
	else if (sample < -MAD_F_ONE)
		sample = -MAD_F_ONE;

	/* quantize */
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static int synth_pcm(struct aic_audio_decoder *decoder,struct mad_synth *synth)
{
	int nchannels, nsamples;
	mad_fixed_t const *left_ch, *right_ch;;
	char *data;
	int pos = 0;
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;
	struct aic_audio_frame *frame;

	nchannels = synth->pcm.channels;
	nsamples = synth->pcm.length;
	left_ch = synth->pcm.samples[0];
	right_ch = synth->pcm.samples[1];
	frame = audio_fm_decoder_get_frame(mp3_decoder->decoder.fm);
	frame->channels =  mp3_decoder->channels;
	frame->sample_rate = mp3_decoder->sample_rate;
	frame->pts = mp3_decoder->curr_packet_info.pts;
	frame->bits_per_sample = mp3_decoder->bits_per_sample;
	frame->id = mp3_decoder->frame_id++;
	frame->flag = mp3_decoder->curr_packet_info.flag;
	data = (char *)frame->data;
	while (nsamples--) {
		signed int sample;
		// output sample(s) in 16-bit signed little-endian PCM
		sample = scale(*left_ch++);
		data[pos++] = (sample>>0) & 0xff;
		data[pos++] = (sample>>8) & 0xff;
		if (nchannels == 2) {
			sample = scale(*right_ch++);
			data[pos++] = (sample>>0) & 0xff;
			data[pos++] = (sample>>8) & 0xff;
		}
	}
	audio_fm_decoder_put_frame(mp3_decoder->decoder.fm, frame);
	return 0;
}

int decode_final_frame(struct aic_audio_decoder *decoder)
{
	int ret;
	int unproc_data_len = 0;
	int need_proc_data_len = 0;
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;

	if ((mp3_decoder->decoder.fm) && (audio_fm_get_empty_frame_num(mp3_decoder->decoder.fm)) == 0) {
		return DEC_NO_EMPTY_FRAME;
	}
	unproc_data_len = mp3_decoder->stream.bufend - mp3_decoder->stream.next_frame;
	if (unproc_data_len <= 0) {
		printf("something wrong happen\n");
		return DEC_NO_RENDER_FRAME;
	}
	memcpy(mp3_decoder->mad_buffer,mp3_decoder->stream.next_frame,unproc_data_len);
	need_proc_data_len = unproc_data_len + unproc_data_len;
	mad_stream_buffer(&mp3_decoder->stream,mp3_decoder->mad_buffer,need_proc_data_len);
	ret = mad_frame_decode(&mp3_decoder->frame,&mp3_decoder->stream);
	if(ret != MAD_ERROR_NONE) {
		printf("%s\n",mad_stream_errorstr(&mp3_decoder->stream));
		printf("something wrong happen\n");
		return DEC_NO_RENDER_FRAME;
	}
	mad_synth_frame(&mp3_decoder->synth,&mp3_decoder->frame);
	mp3_decoder->channels = mp3_decoder->synth.pcm.channels;
	mp3_decoder->sample_rate = mp3_decoder->synth.pcm.samplerate;
	mp3_decoder->bits_per_sample = 16;//fixed
	if (mp3_decoder->decoder.fm == NULL) {
		int nchannels, nsamples;
		struct audio_frame_manager_cfg cfg;
		nchannels =  mp3_decoder->synth.pcm.channels;
		nsamples = mp3_decoder->synth.pcm.length;
		cfg.bits_per_sample = mp3_decoder->bits_per_sample;
		cfg.samples_per_frame = nchannels*nsamples;
		cfg.frame_count = mp3_decoder->frame_count;
		mp3_decoder->decoder.fm = audio_fm_create(&cfg);
		if (mp3_decoder->decoder.fm == NULL) {
			loge("audio_fm_create fail!!!\n");
			return DEC_ERR_NULL_PTR;
		}
	}
	mp3_decoder->final_frame = 0;
	mp3_decoder->curr_packet_info.flag |= PACKET_FLAG_EOS;
	synth_pcm(decoder,&mp3_decoder->synth);
	return DEC_OK;
}

int __mp3_decode_frame(struct aic_audio_decoder *decoder)
{
	int ret;
	int unproc_data_len = 0;
	int need_proc_data_len = 0;
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;

	if (audio_pm_get_ready_packet_num(mp3_decoder->decoder.pm) == 0) {
		if (mp3_decoder->final_frame == 1) {
			struct aic_audio_frame *frame;
			ret = decode_final_frame(decoder);
			if (ret == DEC_NO_EMPTY_FRAME || ret== DEC_OK) {
				return ret;
			}
			if(ret == DEC_ERR_NULL_PTR) {
				mp3_decoder->final_frame = 0;
				return ret;
			}
			//Decoding the last frame failed,zero data will be used instead
			printf("Decoding the last frame failed,zero data will be used instead\n");
			frame = audio_fm_decoder_get_frame(mp3_decoder->decoder.fm);
			memset(frame->data,0x00,frame->size);
			frame->flag |= PACKET_FLAG_EOS;
			audio_fm_decoder_put_frame(mp3_decoder->decoder.fm, frame);
			mp3_decoder->final_frame = 0;
			return DEC_OK;
		}
		return DEC_NO_READY_PACKET;
	}
	if ((mp3_decoder->decoder.fm) && (audio_fm_get_empty_frame_num(mp3_decoder->decoder.fm)) == 0) {
		return DEC_NO_EMPTY_FRAME;
	}
	mp3_decoder->curr_packet = audio_pm_dequeue_ready_packet(mp3_decoder->decoder.pm);
	unproc_data_len = mp3_decoder->stream.bufend - mp3_decoder->stream.next_frame;
	if (mp3_decoder->curr_packet->size > mp3_decoder->mad_buffer_len - unproc_data_len) {
		loge("curr_packet_size:%d,mad_buffer_len:%d,unproc_data_len:%d\n"
		,mp3_decoder->curr_packet->size,mp3_decoder->mad_buffer_len,unproc_data_len);
		return DEC_ERR_NULL_PTR;
	}
	if (unproc_data_len > 0) {
		memcpy(mp3_decoder->mad_buffer,mp3_decoder->stream.next_frame,unproc_data_len);
	}
	memcpy(mp3_decoder->mad_buffer + unproc_data_len,mp3_decoder->curr_packet->data,mp3_decoder->curr_packet->size);
	mp3_decoder->curr_packet_info.flag = mp3_decoder->curr_packet->flag;
	mp3_decoder->curr_packet_info.pts = mp3_decoder->curr_packet->pts;
	audio_pm_enqueue_empty_packet(mp3_decoder->decoder.pm, mp3_decoder->curr_packet);

	need_proc_data_len = unproc_data_len + mp3_decoder->curr_packet->size;
	mad_stream_buffer(&mp3_decoder->stream,mp3_decoder->mad_buffer,need_proc_data_len);
	ret = mad_frame_decode(&mp3_decoder->frame,&mp3_decoder->stream);
	if (ret != MAD_ERROR_NONE) {
		if (mp3_decoder->stream.error != MAD_ERROR_BUFLEN)
			printf("%s\n",mad_stream_errorstr(&mp3_decoder->stream));
		return DEC_NO_RENDER_FRAME;
	}

	mad_synth_frame(&mp3_decoder->synth,&mp3_decoder->frame);

	mp3_decoder->channels = mp3_decoder->synth.pcm.channels;
	mp3_decoder->sample_rate = mp3_decoder->synth.pcm.samplerate;
	mp3_decoder->bits_per_sample = 16;//fixed

	if (mp3_decoder->decoder.fm == NULL) {
		int nchannels, nsamples;
		struct audio_frame_manager_cfg cfg;
		nchannels =  mp3_decoder->synth.pcm.channels;
		nsamples = mp3_decoder->synth.pcm.length;
		cfg.bits_per_sample = mp3_decoder->bits_per_sample;
		cfg.samples_per_frame = nchannels*nsamples;
		cfg.frame_count = mp3_decoder->frame_count;
		mp3_decoder->decoder.fm = audio_fm_create(&cfg);
		if (mp3_decoder->decoder.fm == NULL) {
			loge("audio_fm_create fail!!!\n");
			return DEC_ERR_NULL_PTR;
		}
	}

	if (mp3_decoder->curr_packet_info.flag & PACKET_FLAG_EOS) {
		mp3_decoder->final_frame = 1;
		mp3_decoder->curr_packet_info.flag = 0;
	}

	synth_pcm(decoder,&mp3_decoder->synth);

	return DEC_OK;
}

int __mp3_decode_control(struct aic_audio_decoder *decoder, int cmd, void *param)
{
	return 0;
}

int __mp3_decode_reset(struct aic_audio_decoder *decoder)
{
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;
	mad_frame_finish(&mp3_decoder->frame);
	mad_stream_finish(&mp3_decoder->stream);
	mad_synth_finish(&mp3_decoder->header);

	mad_stream_init(&mp3_decoder->stream);
	mad_frame_init(&mp3_decoder->frame);
	mad_synth_init(&mp3_decoder->synth);

	audio_pm_reset(mp3_decoder->decoder.pm);
	audio_fm_reset(mp3_decoder->decoder.fm);
	return 0;
}


struct aic_audio_decoder_ops mp3_decoder = {
	.name		   = "mp3",
	.init		   = __mp3_decode_init,
	.destroy		= __mp3_decode_destroy,
	.decode		 = __mp3_decode_frame,
	.control		= __mp3_decode_control,
	.reset		  = __mp3_decode_reset,
};

struct aic_audio_decoder* create_mp3_decoder()
{
	struct mp3_audio_decoder *s = (struct mp3_audio_decoder*)mpp_alloc(sizeof(struct mp3_audio_decoder));
	if(s == NULL)
		return NULL;
	memset(s, 0, sizeof(struct mp3_audio_decoder));
	s->decoder.ops = &mp3_decoder;
	return &s->decoder;
}
