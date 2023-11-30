/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
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
	int  curr_packet_unused_len;
	struct mad_decoder *mad_handle;
	unsigned char *mad_buffer;
	char *mad_buffer_end;
	int mad_buffer_len;
	int channels;
	int sample_rate;
	int bits_per_sample;
	int frame_id;
	int stream_end;
	int frame_count;
	pthread_t decode_thread_id;
	int stop_flag;
};

static enum mad_flow input(void *priv, struct mad_stream *stream)
{
	struct mp3_audio_decoder *mp3_dcecoder = (struct mp3_audio_decoder *)priv;
	int unproc_data_len = 0;
	int need_proc_data_len = 0;

	if (mp3_dcecoder->stop_flag == 1) {
		logd("MAD_FLOW_STOP!!!!\n");
		return MAD_FLOW_STOP;
	}

	unproc_data_len = stream->bufend - stream->next_frame;

	// 1 get unprocess data and data len,move unporocess data to buffer start.
	if (unproc_data_len > 0) {
		memcpy(mp3_dcecoder->mad_buffer,stream->next_frame,unproc_data_len);
	}

	// 2
	if (mp3_dcecoder->curr_packet_unused_len > 0) {
		if (mp3_dcecoder->curr_packet_unused_len + unproc_data_len <= MAD_BUFFER_LEEN) {
			memcpy(mp3_dcecoder->mad_buffer+unproc_data_len
					,mp3_dcecoder->curr_packet->data+mp3_dcecoder->curr_packet->size - mp3_dcecoder->curr_packet_unused_len
					,mp3_dcecoder->curr_packet_unused_len);
			need_proc_data_len = mp3_dcecoder->curr_packet_unused_len + unproc_data_len;
			mp3_dcecoder->curr_packet_unused_len = 0;
			audio_pm_enqueue_empty_packet(mp3_dcecoder->decoder.pm, mp3_dcecoder->curr_packet);
			logd("need_proc_data_len:%d,unproc_data_len:%d,curr_packet_unused_len:%d\n",need_proc_data_len,unproc_data_len,mp3_dcecoder->curr_packet_unused_len);
		} else {
			memcpy(mp3_dcecoder->mad_buffer+unproc_data_len
					,mp3_dcecoder->curr_packet->data+mp3_dcecoder->curr_packet->size - mp3_dcecoder->curr_packet_unused_len
					,MAD_BUFFER_LEEN -unproc_data_len);

			mp3_dcecoder->curr_packet_unused_len -= (MAD_BUFFER_LEEN - unproc_data_len);
			need_proc_data_len = MAD_BUFFER_LEEN;
			logd("need_proc_data_len:%d,unproc_data_len:%d,curr_packet_unused_len:%d\n",need_proc_data_len,unproc_data_len,mp3_dcecoder->curr_packet_unused_len);
		}
		mad_stream_buffer(stream, mp3_dcecoder->mad_buffer,need_proc_data_len);
		if ((mp3_dcecoder->curr_packet->flag & PACKET_FLAG_EOS) && (mp3_dcecoder->curr_packet_unused_len == 0)) {
			//logd("PACKET_FLAG_EOS--->MAD_FLOW_STOP!!!!\n");
			mp3_dcecoder->stop_flag = 1;
		}
		return MAD_FLOW_CONTINUE;
	}

	// get ready packet
	while((mp3_dcecoder->curr_packet = audio_pm_dequeue_ready_packet(mp3_dcecoder->decoder.pm)) == NULL) {
		if (mp3_dcecoder->stop_flag == 1) {// force to stop
			logd("force to stop!!!\n");
			return MAD_FLOW_STOP;
		}
		usleep(10*1000);// how to do  sleep
	}
	mp3_dcecoder->curr_packet_info.flag = mp3_dcecoder->curr_packet->flag;
	mp3_dcecoder->curr_packet_info.pts = mp3_dcecoder->curr_packet->pts;

	if (unproc_data_len + mp3_dcecoder->curr_packet->size <= MAD_BUFFER_LEEN) {
		memcpy(mp3_dcecoder->mad_buffer+unproc_data_len
					,mp3_dcecoder->curr_packet->data
					,mp3_dcecoder->curr_packet->size);
		need_proc_data_len = unproc_data_len + mp3_dcecoder->curr_packet->size;
		mp3_dcecoder->curr_packet_unused_len = 0;
		audio_pm_enqueue_empty_packet(mp3_dcecoder->decoder.pm, mp3_dcecoder->curr_packet);
		logd("need_proc_data_len:%d,unproc_data_len:%d,curr_packet_unused_len:%d\n",need_proc_data_len,unproc_data_len,mp3_dcecoder->curr_packet_unused_len);

	} else {
		memcpy(mp3_dcecoder->mad_buffer+unproc_data_len
					,mp3_dcecoder->curr_packet->data
					,MAD_BUFFER_LEEN -unproc_data_len);
		mp3_dcecoder->curr_packet_unused_len = mp3_dcecoder->curr_packet->size - (MAD_BUFFER_LEEN - unproc_data_len);
		need_proc_data_len = MAD_BUFFER_LEEN;
		logd("need_proc_data_len:%d,unproc_data_len:%d,curr_packet_unused_len:%d\n",need_proc_data_len,unproc_data_len,mp3_dcecoder->curr_packet_unused_len);
	}

	// send ready packet to libmad decdoer
	mad_stream_buffer(stream, mp3_dcecoder->mad_buffer, need_proc_data_len);

	if ((mp3_dcecoder->curr_packet_info.flag & PACKET_FLAG_EOS) && (mp3_dcecoder->curr_packet_unused_len == 0)) {
		logd("stream_end!!!!\n");
		mp3_dcecoder->stop_flag = 1;
	}

	logd("input packet flag:%u,pts:%lld,data:%p,size:%d\n"
		,mp3_dcecoder->curr_packet->flag
		,mp3_dcecoder->curr_packet->pts
		,mp3_dcecoder->curr_packet->data
		,mp3_dcecoder->curr_packet->size);
	return MAD_FLOW_CONTINUE;
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

static enum mad_flow output(void *priv, struct mad_header const *header, struct mad_pcm *pcm)
{
	enum mad_flow ret = MAD_FLOW_CONTINUE;
	unsigned int nchannels, nsamples;
	mad_fixed_t const *left_ch, *right_ch;
	struct mp3_audio_decoder *mp3_dcecoder = (struct mp3_audio_decoder *)priv;
	int pos = 0;
	struct mpp_packet;
	struct aic_audio_frame *frame;
	s8 *data = NULL;

	mp3_dcecoder->channels = pcm->channels;
	mp3_dcecoder->sample_rate = pcm->samplerate;
	mp3_dcecoder->bits_per_sample = 16;//fixed
	nchannels = pcm->channels;
	nsamples = pcm->length;
	left_ch = pcm->samples[0];
	right_ch = pcm->samples[1];
	int data_size = nchannels * nsamples * mp3_dcecoder->bits_per_sample/8;
	if (mp3_dcecoder->decoder.fm == NULL) {
		struct audio_frame_manager_cfg cfg;
		cfg.bits_per_sample = mp3_dcecoder->bits_per_sample;
		cfg.samples_per_frame = nchannels*nsamples;
		cfg.frame_count = mp3_dcecoder->frame_count;
		mp3_dcecoder->decoder.fm = audio_fm_create(&cfg);
		if (mp3_dcecoder->decoder.fm == NULL) {
			loge("audio_fm_create fail!!!\n");
			return MAD_FLOW_STOP;
		}
	}

	//get empty frame
	while((frame = audio_fm_decoder_get_frame(mp3_dcecoder->decoder.fm)) == NULL) {
		if (mp3_dcecoder->stop_flag == 1) {// force to stop
			printf("[%s:%d] force to stop!!!\n",__FUNCTION__,__LINE__);
			return MAD_FLOW_STOP;
		}
		usleep(10*1000);// how to do  sleep
	}

	if (frame->size < data_size) {
		if (frame->data) {
			logi("frame->data realloc!!\n");
			mpp_free(frame->data);
			frame->data = NULL;
			frame->data = mpp_alloc(data_size);
			if (frame->data == NULL) {
				loge("mpp_alloc frame->data fail!!!\n");
				return MAD_FLOW_STOP;
			}
			frame->size = data_size;
		}
	}
	frame->channels =  pcm->channels;
	frame->sample_rate = pcm->samplerate;
	frame->pts =  mp3_dcecoder->curr_packet_info.pts;// how to calculate
	frame->bits_per_sample = mp3_dcecoder->bits_per_sample;
	frame->id = mp3_dcecoder->frame_id++;
	frame->flag = mp3_dcecoder->curr_packet_info.flag;

	logd("output frame,"\
		"bits_per_sample:%d,"\
		"channels:%d,"\
		"flag:0x%x,"\
		"pts:%"PRId64",sample_rate:%d,"\
		"frame_id:%d,"\
		"size:%d\n"\
		,frame->bits_per_sample
		,frame->channels
		,frame->flag
		,frame->pts
		,frame->sample_rate
		,frame->id
		,frame->size);

	data = (s8 *)frame->data;
	while (nsamples--) {
		signed int sample;
		/* output sample(s) in 16-bit signed little-endian PCM */
		sample = scale(*left_ch++);
		data[pos++] = (sample>>0) & 0xff;
		data[pos++] = (sample>>8) & 0xff;

		if (nchannels == 2) {
			sample = scale(*right_ch++);
			data[pos++] = (sample>>0) & 0xff;
			data[pos++] = (sample>>8) & 0xff;
		}
	}
	if (audio_fm_decoder_put_frame(mp3_dcecoder->decoder.fm, frame) != 0) {
		loge("plese check code,why!!!\n");
		return MAD_FLOW_STOP;
	}
	return ret;
}

static enum mad_flow error(void *priv, struct mad_stream *stream, struct mad_frame *frame)
{
	struct mp3_audio_decoder *mp3_dcecoder = (struct mp3_audio_decoder *)priv;
	/* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */
	//decode error
	//audio_pm_reclaim_ready_packet(mp3_dcecoder->decoder.pm,mp3_dcecoder->curr_packet);
	//audio_pm_enqueue_empty_packet(mp3_dcecoder->decoder.pm, mp3_dcecoder->curr_packet);
	loge("error packet flag:0x%x,pts:%lld,data:%p,size:%d,error:%d\n"
	,mp3_dcecoder->curr_packet->flag
	,mp3_dcecoder->curr_packet->pts
	,mp3_dcecoder->curr_packet->data
	,mp3_dcecoder->curr_packet->size
	,stream->error);
	mad_stream_errorstr(stream);
	return MAD_FLOW_CONTINUE;
}

int __mp3_decode_init(struct aic_audio_decoder *decoder, struct aic_audio_decode_config *config)
{
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;
	mad_decoder_init(mp3_decoder->mad_handle, mp3_decoder, input, NULL,NULL, output, error, NULL);
	mp3_decoder->mad_buffer = mpp_alloc(MAD_BUFFER_LEEN);
	mp3_decoder->decoder.pm = audio_pm_create(config);
	mp3_decoder->frame_count = config->frame_count;
	mp3_decoder->stop_flag = 0;
	mp3_decoder->decode_thread_id = 0;
	//mp3_decoder->decoder.fm = audio_fm_create(config);
	return 0;
}

int __mp3_decode_destroy(struct aic_audio_decoder *decoder)
{
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;

	mp3_decoder->stop_flag = 1;
	pthread_join(mp3_decoder->decode_thread_id, NULL);
	mad_decoder_finish(mp3_decoder->mad_handle);
	mpp_free(mp3_decoder->mad_handle);
	audio_pm_destroy(mp3_decoder->decoder.pm);
	audio_fm_destroy(mp3_decoder->decoder.fm);
	if (mp3_decoder->mad_buffer) {
		mpp_free(mp3_decoder->mad_buffer);
		mp3_decoder->mad_buffer = NULL;
	}
	mpp_free(mp3_decoder);
	return 0;
}

static void* mp3_decode_thread(void *pThreadData)
{
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)pThreadData;

	printf("[%s:%d]mp3_decode_frame start.....\n",__FUNCTION__,__LINE__);
	mad_decoder_run(mp3_decoder->mad_handle, MAD_DECODER_MODE_SYNC);
	printf("[%s:%d]mp3_decode_frame end.....\n",__FUNCTION__,__LINE__);
	return (void *)0;
}

// create a pthread ,call once
int __mp3_decode_frame(struct aic_audio_decoder *decoder)
{
	s32 ret;
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;
	if (!mp3_decoder->decode_thread_id) {
		ret = pthread_create(&mp3_decoder->decode_thread_id, NULL, mp3_decode_thread, mp3_decoder);
		if (ret || !mp3_decoder->decode_thread_id) {
			loge("pthread_create fail!");
			return DEC_ERR_NOT_SUPPORT;
		} else {
			printf("[%s:%d] pthread_create mp3_decode_thread threadId:%ld ok!"
					,__FUNCTION__,__LINE__,(unsigned long)mp3_decoder->decode_thread_id);
		}
	}

	if (!mp3_decoder->decoder.fm) {
		return DEC_ERR_FM_NOT_CREATE;
	}

	if (audio_fm_get_render_frame_num(mp3_decoder->decoder.fm) > 0) {
		return DEC_OK;
	} else if (audio_fm_get_empty_frame_num(mp3_decoder->decoder.fm) == 0) {
		return DEC_NO_EMPTY_FRAME;
	} else if (audio_pm_get_ready_packet_num(mp3_decoder->decoder.pm) == 0) {
		return DEC_NO_READY_PACKET;
	} else {
		return DEC_NO_RENDER_FRAME;
	}
}
int __mp3_decode_control(struct aic_audio_decoder *decoder, int cmd, void *param)
{
	return 0;
}
int __mp3_decode_reset(struct aic_audio_decoder *decoder)
{
	struct mp3_audio_decoder *mp3_decoder = (struct mp3_audio_decoder *)decoder;

	mp3_decoder->stop_flag = 1;
	pthread_join(mp3_decoder->decode_thread_id, NULL);
	mp3_decoder->stop_flag = 0;
	mp3_decoder->decode_thread_id = 0;
	audio_pm_reset(mp3_decoder->decoder.pm);
	audio_fm_reset(mp3_decoder->decoder.fm);
	return 0;
}

struct aic_audio_decoder_ops mp3_decoder = {
	.name           = "mp3",
	.init           = __mp3_decode_init,
	.destroy        = __mp3_decode_destroy,
	.decode         = __mp3_decode_frame,
	.control        = __mp3_decode_control,
	.reset          = __mp3_decode_reset,
};

struct aic_audio_decoder* create_mp3_decoder()
{
	struct mp3_audio_decoder *s = (struct mp3_audio_decoder*)mpp_alloc(sizeof(struct mp3_audio_decoder));
	if (s == NULL)
		return NULL;
	memset(s, 0, sizeof(struct mp3_audio_decoder));
	s->mad_handle = (struct mad_decoder*)mpp_alloc(sizeof(struct mad_decoder));
	if (s->mad_handle == NULL) {
		mpp_free(s);
		return NULL;
	}
	s->decoder.ops = &mp3_decoder;
	return &s->decoder;
}
