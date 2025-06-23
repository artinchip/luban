// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2024 ArtInChip Technology Co., Ltd.
 * Author: weijie.ding <weijie.ding@artinchip.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <locale.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <poll.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <endian.h>

#include "goertzel.h"

int16_t audio_rx_data[480];
static snd_pcm_t *play_handle;
static snd_pcm_t *record_handle;

/* 1KHz sin wave */
int16_t audio_tx_data[480] = {
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
	0x10B5, 0x2121, 0x30FC, 0x4000, 0x4DEC, 0x5A82, 0x658D, 0x6EDA,
	0x7642, 0x7BA3, 0x7EE8, 0x7FFF, 0x7EE8, 0x7BA3, 0x7642, 0x6EDA,
	0x658D, 0x5A82, 0x4DEC, 0x4000, 0x30FC, 0x2121, 0x10B5, 0x0000,
	0xEF4B, 0xDEDF, 0xCF04, 0xC000, 0xB214, 0xA57E, 0x9A73, 0x9126,
	0x89BE, 0x845D, 0x8118, 0x8001, 0x8118, 0x845D, 0x89BE, 0x9126,
	0x9A73, 0xA57E, 0xB214, 0xC000, 0xCF04, 0xDEDF, 0xEF4B, 0x0000,
};

int playback_init(void)
{
	snd_pcm_hw_params_t *hwparams = NULL;
	int ret;

	ret = snd_pcm_open(&play_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (ret < 0) {
		printf("audio open error: %s\n", snd_strerror(ret));
		return 1;
	}

	snd_pcm_hw_params_alloca(&hwparams);

	ret = snd_pcm_hw_params_any(play_handle, hwparams);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_any error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_access(play_handle, hwparams,
					   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_access error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_format(play_handle, hwparams,
					   SND_PCM_FORMAT_S16_LE);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_format error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_rate(play_handle, hwparams, 48000, 0);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_rate error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_channels(play_handle, hwparams, 1);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_channels error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_period_size(play_handle, hwparams, 480, 0);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_period_size error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_periods(play_handle, hwparams, 16, 0);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_periods error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params(play_handle, hwparams);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params error: %s\n",
			snd_strerror(ret));
		goto err1;
	}

	return 0;
err2:
	snd_pcm_hw_params_free(hwparams);
err1:
	snd_pcm_close(play_handle);
	return -1;
}

int record_init(void)
{
	snd_pcm_hw_params_t *hwparams = NULL;
	int ret;

	ret = snd_pcm_open(&record_handle, "hw:0,1", SND_PCM_STREAM_CAPTURE, 0);
	if (ret < 0) {
		printf("audio open error: %s\n", snd_strerror(ret));
		return 1;
	}

	snd_pcm_hw_params_malloc(&hwparams);

	ret = snd_pcm_hw_params_any(record_handle, hwparams);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_any error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_access(record_handle, hwparams,
					   SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_access error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_format(record_handle, hwparams,
					   SND_PCM_FORMAT_S16_LE);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_format error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_rate(record_handle, hwparams, 48000, 0);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_rate error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_channels(record_handle, hwparams, 1);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_channels error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_period_size(record_handle, hwparams,
						480, 0);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_period_size error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params_set_periods(record_handle, hwparams, 16, 0);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params_set_periods error: %s\n",
			snd_strerror(ret));
		goto err2;
	}

	ret = snd_pcm_hw_params(record_handle, hwparams);
	snd_pcm_hw_params_free(hwparams);
	if (ret < 0) {
		fprintf(stderr, "snd_pcm_hw_params error: %s\n",
			snd_strerror(ret));
		goto err1;
	}
	return 0;
err2:
	snd_pcm_hw_params_free(hwparams);
err1:
	snd_pcm_close(record_handle);
	return -1;
}

void * record_thread(void *arg)
{
	int j = 20;
	int rframe;

	while (j--) {
		rframe = snd_pcm_readi(record_handle, audio_rx_data, 480);
		if (rframe < 0)
			break;
	}

	snd_pcm_close(record_handle);

	return NULL;
}

void * playback_thread(void *arg)
{
	int wframe;
	while (1) {
		wframe = snd_pcm_writei(play_handle,
					(void *)audio_tx_data, 480);
		if (wframe < 0)
			break;
	}

	return NULL;
}

int main(int argc, char *argv[])
{
	int err;
	pthread_t tid1 = 0;
	pthread_t tid2 = 0;
	void *result;

	system("amixer sset \'PGA Gain\' 0");
	system("amixer sset \'ADC HPF\' \'HPF Enable\'");

	err = playback_init();
	if (err) {
		printf("playback init failed\n");
		return err;
	}

	err = record_init();
	if (err) {
		printf("record init failed\n");
		return err;
	}

	memset(audio_rx_data, 0, sizeof(audio_rx_data));

	pthread_create(&tid1, NULL, record_thread, NULL);
	pthread_create(&tid2, NULL, playback_thread, NULL);

	pthread_join(tid1, &result);
	snd_pcm_close(play_handle);

	err = goertzel_test();
	printf("audio QC test result: %d\n", err);

	return err;
}

