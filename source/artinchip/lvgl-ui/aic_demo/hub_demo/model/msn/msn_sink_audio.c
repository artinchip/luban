/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors: artinchip
 */

#ifdef USE_MSNLINK
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <alsa/asoundlib.h>
#include <mpp_dec_type.h>

#include "mpp_log.h"
#include "msn_sink_audio.h"

#define DEV_SOUND_DEFAULT "default"

struct sink_audio_attr {
    s32 channels;
    s32 sample_rate;
    s32 bits_per_sample;
    s32 smples_per_frame;
};

struct sink_audio_ctx{
    struct sink_audio_attr attr;
    snd_pcm_t      *alsa_handle;
    snd_pcm_hw_params_t *alse_hw_param;
    snd_mixer_t *alsa_mixer_handle;
    snd_mixer_elem_t *alsa_mixer_elem;
    long vol;
    int status;
};

static struct sink_audio_ctx *g_p_sink_audio_ctx = NULL;

static s32 asla_volum_init(struct sink_audio_ctx *ctx)
{
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    const char *card = "default";
    const char *selem_name = "AUDIO";
    struct sink_audio_ctx *p_sink_audio_ctx = (struct sink_audio_ctx*)ctx;
    long vol = 0;
    if(!p_sink_audio_ctx) {
        loge("param error!!!\n");
        return -1;
    }

    snd_mixer_open(&handle, 0);
    if(!handle) {
        loge("snd_mixer_open error!!!\n");
        return -1;
    }
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);
    elem = snd_mixer_find_selem(handle, sid);
    if (!elem) {
        loge("Can't find volume elem\n");
        return -1;
    }

    snd_mixer_selem_get_playback_volume(elem,0,&vol);
    logd("vol:[%ld]\n",vol);
    p_sink_audio_ctx->vol = vol;
    p_sink_audio_ctx->alsa_mixer_handle = handle;
    p_sink_audio_ctx->alsa_mixer_elem = elem;
    return 0;
}

static s32 alsa_audio_render_init(struct sink_audio_ctx *ctx)
{
    struct sink_audio_ctx *p_sink_audio_ctx = (struct sink_audio_ctx*)ctx;

    if (p_sink_audio_ctx == NULL) {
        loge("param error!!!\n");
        return -1;
    }

    if (snd_pcm_open(&p_sink_audio_ctx->alsa_handle, DEV_SOUND_DEFAULT, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        loge("snd_pcm_open failed!!!\n");
        return -1;
    }

    if (asla_volum_init(p_sink_audio_ctx) != 0) {
        loge("asla_volum_init error\n");
        return -1;
    }

    return 0;
}


static s32 alsa_audio_render_set_attr(struct sink_audio_ctx *ctx,struct sink_audio_attr *attr)
{
    u32 rate;
    snd_pcm_uframes_t period_size = 32 * 1024;
    snd_pcm_hw_params_t *alse_hw_param;

    struct sink_audio_ctx *p_sink_audio_ctx = (struct sink_audio_ctx*)ctx;
    if (p_sink_audio_ctx == NULL || attr == NULL) {
        loge("param error!!!\n");
        return -1;
    }
    p_sink_audio_ctx->attr.bits_per_sample = attr->bits_per_sample;
    p_sink_audio_ctx->attr.channels = attr->channels;
    p_sink_audio_ctx->attr.sample_rate = attr->sample_rate;
    p_sink_audio_ctx->attr.smples_per_frame = attr->smples_per_frame;
    logd("bits_per_sample:%d,"\
         "channels:%d,"\
         "sample_rate:%d,"\
         "smples_per_frame:%d\n"\
         ,p_sink_audio_ctx->attr.bits_per_sample
         ,p_sink_audio_ctx->attr.channels
         ,p_sink_audio_ctx->attr.sample_rate
         ,p_sink_audio_ctx->attr.smples_per_frame);

    if (!p_sink_audio_ctx->alse_hw_param) {
        snd_pcm_hw_params_malloc(&alse_hw_param);
        p_sink_audio_ctx->alse_hw_param = alse_hw_param;
        logd("snd_pcm_hw_params_malloc\n");
    } else {
        return 0;
    }

    if (snd_pcm_hw_params_any(p_sink_audio_ctx->alsa_handle, p_sink_audio_ctx->alse_hw_param) < 0) {
        loge("snd_pcm_hw_params_any failed!!!\n");
        return -1;
    }

    if (snd_pcm_hw_params_set_access(p_sink_audio_ctx->alsa_handle, p_sink_audio_ctx->alse_hw_param,SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
        loge("snd_pcm_hw_params_set_access failed!!!\n");
        return -1;
    }

    if (snd_pcm_hw_params_set_format(p_sink_audio_ctx->alsa_handle, p_sink_audio_ctx->alse_hw_param, SND_PCM_FORMAT_S16_LE) < 0) {
        loge("snd_pcm_hw_params_set_format failed!!!\n");
        return -1;
    }

    if (snd_pcm_hw_params_set_channels(p_sink_audio_ctx->alsa_handle, p_sink_audio_ctx->alse_hw_param, p_sink_audio_ctx->attr.channels) < 0) {
        loge("snd_pcm_hw_params_set_channels failed!!!\n");
        return -1;
    }
    rate = p_sink_audio_ctx->attr.sample_rate;
    if (snd_pcm_hw_params_set_rate_near(p_sink_audio_ctx->alsa_handle, p_sink_audio_ctx->alse_hw_param, &rate, 0) < 0) {
        loge("snd_pcm_hw_params_set_rate_near failed!!!\n");
        return -1;
    }
    if ((float)p_sink_audio_ctx->attr.sample_rate * 1.05 < rate || (float)p_sink_audio_ctx->attr.sample_rate * 0.95 > rate) {
        logw("rate is not accurate(request: %d, got: %d)", p_sink_audio_ctx->attr.sample_rate, rate);
    }

    period_size = p_sink_audio_ctx->attr.smples_per_frame;

    if (snd_pcm_hw_params_set_period_size_near(p_sink_audio_ctx->alsa_handle, p_sink_audio_ctx->alse_hw_param, &period_size, 0) < 0) {
        loge("snd_pcm_hw_params_set_period_size_near failed!!!\n");
        return -1;
    }

    period_size = 4 * period_size;

    if (snd_pcm_hw_params_set_buffer_size_near(p_sink_audio_ctx->alsa_handle, p_sink_audio_ctx->alse_hw_param, &period_size) < 0) {
        loge("snd_pcm_hw_params_set_period_size_near failed!!!\n");
        return -1;
    }

    if (snd_pcm_hw_params(p_sink_audio_ctx->alsa_handle, p_sink_audio_ctx->alse_hw_param) < 0) {
        loge("Unable to install hw params:");
        return -1;
    }

    return 0;

}

static s32 alsa_audio_render_rend(struct sink_audio_ctx *render, void* pData, s32 nDataSize)
{
    struct sink_audio_ctx *p_sink_audio_ctx = (struct sink_audio_ctx*)render;
    s32 ret = 0;
    s32 count = 0;
    s32 pos = 0;

    if (p_sink_audio_ctx == NULL || pData == NULL || nDataSize == 0) {
        loge("param error!!!\n");
        return -1;
    }
    count = nDataSize/(p_sink_audio_ctx->attr.channels*p_sink_audio_ctx->attr.bits_per_sample/8);
    while (count > 0) {
        ret = snd_pcm_writei(p_sink_audio_ctx->alsa_handle, pData+pos, count);
        if (ret == -EAGAIN || (ret>=0 && ret < count)) {
            snd_pcm_wait(p_sink_audio_ctx->alsa_handle, 100);
            logi("wait 100!!!\n");
        } else if(ret == -EPIPE) {// underrun
            ret = snd_pcm_prepare(p_sink_audio_ctx->alsa_handle);
            if (ret < 0)
                loge("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(ret));
            ret = 0;
            logi("snd_pcm_prepare!!!\n");
        } else if(ret == -ESTRPIPE) {
            while ((ret = snd_pcm_resume(p_sink_audio_ctx->alsa_handle)) == -EAGAIN) {
                usleep(1000);
                logi("snd_pcm_resume!!!\n");
            }
        } else if(ret < 0) {
            loge("write error, ret: %d", ret);
            return -1;
        }
        //logi("ret:%d\n",ret);
        if(ret) {
            count -= ret;
            pos += ret * p_sink_audio_ctx->attr.channels*p_sink_audio_ctx->attr.bits_per_sample/8;
        }
    }
    return 0;

}

// static s32 alsa_audio_render_set_volume(struct sink_audio_ctx *ctx,s32 vol)
// {
//  long min, max;
//  struct sink_audio_ctx *p_sink_audio_ctx = (struct sink_audio_ctx*)ctx;
//  logd("alsa_audio_render_set_volume:%d\n",vol);
//  if(p_sink_audio_ctx->alsa_mixer_handle && p_sink_audio_ctx->alsa_mixer_elem){
//      snd_mixer_selem_get_playback_volume_range(p_sink_audio_ctx->alsa_mixer_elem, &min, &max);
//      vol = vol*(max-min)/100; //[0,100] ---> [min,max]
//      logd("vol:[%ld,%ld:%d]\n",min,max,vol);
//      snd_mixer_selem_set_playback_volume_all(p_sink_audio_ctx->alsa_mixer_elem, vol);
//      p_sink_audio_ctx->vol = vol;
//  }else{
//      loge("alsa_mixer_handle or alsa_mixer_elem are null \n");
//      return -1;
//  }
//  return 0;
// }

void aic_audio_start(LinkType type, int streamType, int audioType, int rate, int format, int channel)
{
    struct sink_audio_ctx * p_sink_audio_ctx = NULL;
    struct sink_audio_attr  audio_attr;

    p_sink_audio_ctx = (struct sink_audio_ctx *)malloc(sizeof(struct sink_audio_ctx));
    if (p_sink_audio_ctx == NULL) {
        loge("mpp_alloc sink_audio_ctx fail!!!\n");
        return;
    }
    memset(p_sink_audio_ctx,0x00,sizeof(struct sink_audio_ctx));
    alsa_audio_render_init(p_sink_audio_ctx);
    audio_attr.bits_per_sample = 16;
    audio_attr.channels = channel;
    audio_attr.sample_rate = rate;
    audio_attr.smples_per_frame = 1024;
    alsa_audio_render_set_attr(p_sink_audio_ctx,&audio_attr);
    //alsa_audio_render_set_volume(p_sink_audio_ctx,60);
}


void aic_audio_play(LinkType type, int streamType, int audioType, void * datas, int len)
{
    if(!g_p_sink_audio_ctx) {
        loge("g_p_sink_audio_ctx==NULL\n");
        return;
    }
    alsa_audio_render_rend(g_p_sink_audio_ctx, datas, len);
}

void aic_audio_stop(LinkType type, int streamType, int audioType)
{
    if(!g_p_sink_audio_ctx) {
        loge("g_p_sink_audio_ctx==NULL\n");
        return;
    }

    if (g_p_sink_audio_ctx->alsa_handle) {
        if (snd_pcm_drain(g_p_sink_audio_ctx->alsa_handle) < 0) {
            loge("snd_pcm_drain failed");
        }
        if (snd_pcm_close(g_p_sink_audio_ctx->alsa_handle) < 0) {
            loge("snd_pcm_close failed!!!\n");
            return;
        } else {
            g_p_sink_audio_ctx->alsa_handle = NULL;
            logd("snd_pcm_close ok \n");
        }
    }

    if (g_p_sink_audio_ctx->alsa_mixer_handle) {
        snd_mixer_close(g_p_sink_audio_ctx->alsa_mixer_handle);
    }

    free(g_p_sink_audio_ctx);

    g_p_sink_audio_ctx = NULL;
    return;
}
#endif
