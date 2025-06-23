/*
 * Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <qi.xu@artinchip.com>
 *  Desc: mp3 audio player (mp3 decode use libmad)
 */

#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#include "mad.h"

#define LOG_TAG "mp3player"
#define TAG_ERROR	"error  "
#define TAG_WARNING	"warning"
#define TAG_INFO	"info   "
#define TAG_DEBUG	"debug  "
#define TAG_VERBOSE	"verbose"

#define mpp_log(level, tag, fmt, arg...) ({ \
		printf("%s: %s <%s:%d>: "fmt"\n", tag, LOG_TAG, __FUNCTION__, __LINE__, ##arg); \
	})

#define loge(fmt, arg...) mpp_log(LOGL_ERROR, TAG_ERROR, "\033[40;31m"fmt"\033[0m", ##arg)
#define logw(fmt, arg...) mpp_log(LOGL_WARNING, TAG_WARNING, "\033[40;33m"fmt"\033[0m", ##arg)
#define logi(fmt, arg...) mpp_log(LOGL_INFO, TAG_INFO, "\033[40;32m"fmt"\033[0m", ##arg)
#define logd(fmt, arg...) mpp_log(LOGL_DEBUG, TAG_DEBUG, fmt, ##arg)
#define logv(fmt, arg...) mpp_log(LOGL_VERBOSE, TAG_VERBOSE, fmt, ##arg)

#define SAMPLES 44100
#define CHANELS 2

struct audio_ctx {
	unsigned char* start;
	unsigned long length;

	// audio paramter
	int channels;
	int sample_rate;
	int decode_eos;

	// output data
	unsigned char* pcm_buf;		// the buffer of decoded pcm data
	int read_pos;			// read point in pcm_buf, byte unit
	int write_pos;			// write point in pcm_buf, byte unit
	int buf_len;			// max length of pcm_buf
	int valid_size;			// valid data size in pcm_buf
	pthread_mutex_t out_mutex;	// output buffer mutex
	FILE* fp_out;
};

static void print_help(void)
{
	printf("Usage: mp3_test [options]:\n"
		"   -i                             input stream file name\n"
		"   -h                             help\n\n"
		"Example: mpp_test -i test.264\n");
}

static enum mad_flow input(void *data, struct mad_stream *stream)
{
	struct audio_ctx *buffer = data;

	if (!buffer->length)
		return MAD_FLOW_STOP;

	mad_stream_buffer(stream, buffer->start, buffer->length);

	buffer->length = 0;

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

static enum mad_flow output(void *data, struct mad_header const *header, struct mad_pcm *pcm)
{
	unsigned int nchannels, nsamples;
	mad_fixed_t const *left_ch, *right_ch;
	struct audio_ctx *ctx = data;
	int tmp = 0;

	/* pcm->samplerate contains the sampling frequency */

	nchannels = pcm->channels;
	nsamples = pcm->length;
	left_ch = pcm->samples[0];
	right_ch = pcm->samples[1];

	ctx->channels = nchannels;
	ctx->sample_rate = pcm->samplerate;

	int data_size = nchannels * nsamples * sizeof(short);

	while(ctx->buf_len - ctx->valid_size < data_size) {
		loge("empty data not enough");
		usleep(1000);
	}

	tmp = ctx->write_pos;

	while (nsamples--) {
		signed int sample;

		/* output sample(s) in 16-bit signed little-endian PCM */
		sample = scale(*left_ch++);
		ctx->pcm_buf[ctx->write_pos++] = (sample>>0) & 0xff;
		ctx->pcm_buf[ctx->write_pos++] = (sample>>8) & 0xff;
		if(ctx->write_pos >= ctx->buf_len) {
			ctx->write_pos = 0;
		}

		if (nchannels == 2) {
			sample = scale(*right_ch++);
			ctx->pcm_buf[ctx->write_pos++] = (sample>>0) & 0xff;
			ctx->pcm_buf[ctx->write_pos++] = (sample>>8) & 0xff;
			if(ctx->write_pos >= ctx->buf_len) {
				ctx->write_pos = 0;
			}
		}
	}

	pthread_mutex_lock(&ctx->out_mutex);
	ctx->valid_size += data_size;
	pthread_mutex_unlock(&ctx->out_mutex);

	logd("valid_size: %d, read_pos: %d, write_pos: %d\n",
		ctx->valid_size, ctx->read_pos, ctx->write_pos);

#ifdef SAVE_PCM
	fwrite(ctx->pcm_buf + tmp, 1, data_size, ctx->fp_out);
#endif

	return MAD_FLOW_CONTINUE;
}

static enum mad_flow error(void *data, struct mad_stream *stream, struct mad_frame *frame)
{
	struct audio_ctx *buffer = data;

	loge("decoding error 0x%04x (%s) at byte offset %u\n",
		stream->error, mad_stream_errorstr(stream),
		stream->this_frame - buffer->start);

	/* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */

	return MAD_FLOW_CONTINUE;
}

static int set_param(struct audio_ctx* ctx, snd_pcm_t* handle)
{
	snd_pcm_hw_params_t *params;
	int err;

	snd_pcm_hw_params_alloca(&params);
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		loge("Broken configuration for this PCM: no configurations available");
		return -1;
	}

	err = snd_pcm_hw_params_set_access(handle, params,
						   SND_PCM_ACCESS_RW_INTERLEAVED);

	err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		loge("Sample format non available");
		return -1;
	}

	err = snd_pcm_hw_params_set_channels(handle, params, ctx->channels);
	if (err < 0) {
		loge("Channels count non available");
		return -1;
	}

	int rate = ctx->sample_rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
	assert(err >= 0);
	if ((float)ctx->sample_rate * 1.05 < rate || (float)ctx->sample_rate * 0.95 > rate) {
		logw("rate is not accurate(request: %d, got: %d)", ctx->sample_rate, rate);
	}

	snd_pcm_uframes_t period_size = 1024;
	err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, 0);

	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		loge("Unable to install hw params:");
		return -1;
	}

	return 0;
}

void* audio_render_thread(void *p)
{
	struct audio_ctx* ctx = (struct audio_ctx*)p;
	snd_pcm_t* handle;
	int first_frame = 0;
	int byte_per_sample = 2; // 16bit signed
	int pcm_data_len = 0;  // length of pcm data every time to write
	int send_data_len;
	int count;
	int ret;

	ret = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);

	// get decode sample rate
	while(first_frame == 0 && ctx->sample_rate == 0) {
		usleep(1000);
	}

	first_frame = 1;
	pcm_data_len = 12800; //ctx->channels * ctx->sample_rate * byte_per_sample * 20 / 1000; // 5ms
	set_param(ctx, handle);

	logi("samplerate: %d, byte_per_sample: %d, channel: %d",
		ctx->sample_rate, byte_per_sample, ctx->channels);

	while(1) {
		if(ctx->decode_eos && ctx->valid_size < pcm_data_len)
			break;

		// if the decode data is not enough, wait
		if(!ctx->decode_eos && (ctx->valid_size == 0 || ctx->valid_size < pcm_data_len)) {
			usleep(1000);
			continue;
		}

		if(ctx->read_pos <= ctx->write_pos) {
			// send data
			send_data_len = pcm_data_len;
			if(ctx->decode_eos)
				send_data_len = ctx->write_pos - ctx->read_pos;
		} else {
			send_data_len = ctx->buf_len - ctx->read_pos;
		}
		count = send_data_len/(ctx->channels*byte_per_sample);

		unsigned char* ptr = (unsigned char*)ctx->pcm_buf;

		logi("read_pos: %d, send_data_len: %d", ctx->read_pos, send_data_len);

		while(count > 0) {
			ret = snd_pcm_writei(handle, &ctx->pcm_buf[ctx->read_pos], count);
			if(ret == -EAGAIN || (ret>=0 && ret < count)) {
				snd_pcm_wait(handle, 100);
			} else if(ret == -EPIPE) {
				// underrun
				ret = snd_pcm_prepare(handle);
				if (ret < 0)
					printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(ret));
				ret = 0;
			} else if(ret == -ESTRPIPE){
				while ((ret = snd_pcm_resume(handle)) == -EAGAIN)
                			sleep(1);
			} else if(ret < 0) {
				loge("write error, ret: %d", ret);
				return NULL;
			}

			if(ret) {
				count -= ret;
				pthread_mutex_lock(&ctx->out_mutex);
				ctx->read_pos += ret * ctx->channels*byte_per_sample;
				ctx->valid_size -= ret * ctx->channels*byte_per_sample;
				pthread_mutex_unlock(&ctx->out_mutex);
			}
		}

		if(ctx->read_pos >= (ctx->buf_len-1))
			ctx->read_pos = 0;
	}

	snd_pcm_close(handle);
}

int main(int argc, char *argv[])
{
	struct audio_ctx ctx;
	struct mad_decoder decoder;
	FILE* fp_input = NULL;
	int opt;
	int ret = 0;
	memset(&ctx, 0, sizeof(struct audio_ctx));
	pthread_t render_thread_id;

	if(argc < 2) {
		print_help();
		return -1;
	}

	while (1) {
		opt = getopt(argc, argv, "i:h");
		if (opt == -1) {
			break;
		}
		switch (opt) {
		case 'i':
			fp_input = fopen(optarg, "rb");
			if(fp_input == NULL) {
				loge("open file failed");
				return -1;
			}

			fseek(fp_input, 0, SEEK_END);
			ctx.length = ftell(fp_input);
			fseek(fp_input, 0, SEEK_SET);

			ctx.start = malloc(ctx.length);
			fread(ctx.start, ctx.length, 1, fp_input);

			break;
		case 'h':
			print_help();
		default:
			goto out;
		}
	}

	ctx.fp_out = fopen("save.pcm", "wb");
	ctx.buf_len = CHANELS * SAMPLES * 2;
	ctx.pcm_buf = (unsigned char*)malloc(ctx.buf_len);
	ctx.read_pos = 0;
	ctx.write_pos = 0;
	ctx.valid_size = 0;
	pthread_mutex_init(&ctx.out_mutex, NULL);

	mad_decoder_init(&decoder, &ctx, input, 0/* header */,
		0/* filter */, output, error, 0/* message */);

	pthread_create(&render_thread_id, NULL, audio_render_thread, &ctx);

	mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

	mad_decoder_finish(&decoder);

	ctx.decode_eos = 1;
	pthread_join(render_thread_id, NULL);

out:
	if(fp_input)
		fclose(fp_input);
	if(ctx.fp_out)
		fclose(ctx.fp_out);
	if(ctx.start)
		free(ctx.start);
	if(ctx.pcm_buf)
		free(ctx.pcm_buf);

	pthread_mutex_destroy(&ctx.out_mutex);
	return ret;
}
