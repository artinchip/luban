/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <jun.ma@artinchip.com>
*  Desc: aac_decoder
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
#include "faad.h"

struct aac_audio_decoder
{
    struct aic_audio_decoder decoder;
    NeAACDecHandle aac_handle;
    int aac_handle_init_flag;
    struct mpp_packet *curr_packet;
    int channels;
    int samples; // per channel
    int sample_rate;
    int bits_per_sample;
    int frame_id;
    int frame_count;
};

int __aac_decode_init(struct aic_audio_decoder *decoder, struct aic_audio_decode_config *config)
{
    struct aac_audio_decoder *aac_decoder = (struct aac_audio_decoder *)decoder;
    NeAACDecConfigurationPtr aac_cfg;
    aac_decoder->decoder.pm = audio_pm_create(config);
    aac_decoder->frame_count = config->frame_count;
    aac_decoder->aac_handle = NeAACDecOpen();
    aac_cfg = NeAACDecGetCurrentConfiguration(aac_decoder->aac_handle);

    logd("defObjectType:%d,defSampleRate:%lu,"\
         "dontUpSampleImplicitSBR:%d,downMatrix:%d,"\
         "outputFormat:%d,useOldADTSFormat:%d\n"
        ,aac_cfg->defObjectType,aac_cfg->defSampleRate
        ,aac_cfg->dontUpSampleImplicitSBR,aac_cfg->downMatrix
        ,aac_cfg->outputFormat,aac_cfg->useOldADTSFormat);

    aac_cfg->outputFormat = 1;
    aac_cfg->dontUpSampleImplicitSBR = 1;
    NeAACDecSetConfiguration(aac_decoder->aac_handle, aac_cfg);

    return 0;
}

int __aac_decode_destroy(struct aic_audio_decoder *decoder)
{
    struct aac_audio_decoder *aac_decoder = (struct aac_audio_decoder *)decoder;
    audio_pm_destroy(aac_decoder->decoder.pm);
    audio_fm_destroy(aac_decoder->decoder.fm);
    NeAACDecClose(aac_decoder->aac_handle);
    mpp_free(aac_decoder);
    return 0;
}

int __aac_decode_frame(struct aic_audio_decoder *decoder)
{
    s32 ret = 0;
    u8 *pcm_data;
    s32 pcm_data_size;
    unsigned long samplerate;
    unsigned char channels;
    NeAACDecFrameInfo frame_info;
    struct aic_audio_frame *frame;
    struct aac_audio_decoder *aac_decoder = (struct aac_audio_decoder *)decoder;

    if (audio_pm_get_ready_packet_num(aac_decoder->decoder.pm) == 0) {
        return DEC_NO_READY_PACKET;
    }

    if ((aac_decoder->decoder.fm) && (audio_fm_get_empty_frame_num(aac_decoder->decoder.fm)) == 0) {
        return DEC_NO_EMPTY_FRAME;
    }

    if (aac_decoder->aac_handle_init_flag == 0) {
        aac_decoder->curr_packet = audio_pm_dequeue_ready_packet(aac_decoder->decoder.pm);
        ret = NeAACDecInit(aac_decoder->aac_handle, aac_decoder->curr_packet->data, aac_decoder->curr_packet->size, &samplerate, &channels);
        if (ret < 0){
            audio_pm_enqueue_empty_packet(aac_decoder->decoder.pm, aac_decoder->curr_packet);
            loge("NeAACDecInit error\n");
            return DEC_ERR_NOT_SUPPORT;
        }
        aac_decoder->aac_handle_init_flag = 1;
        audio_pm_enqueue_empty_packet(aac_decoder->decoder.pm, aac_decoder->curr_packet);
        return DEC_OK;
    }

    aac_decoder->curr_packet = audio_pm_dequeue_ready_packet(aac_decoder->decoder.pm);
    memset(&frame_info, 0x00, sizeof(NeAACDecFrameInfo));
    pcm_data = NeAACDecDecode(aac_decoder->aac_handle, &frame_info, aac_decoder->curr_packet->data, aac_decoder->curr_packet->size);
    if (frame_info.error != 0) {
        audio_pm_enqueue_empty_packet(aac_decoder->decoder.pm, aac_decoder->curr_packet);
        loge("NeAACDecDecode error\n");
        return DEC_ERR_NOT_SUPPORT;
    }

    if (frame_info.samples == 0){
        audio_pm_enqueue_empty_packet(aac_decoder->decoder.pm, aac_decoder->curr_packet);
        loge("size:%d,pts:%lld\n", aac_decoder->curr_packet->size, aac_decoder->curr_packet->pts);
        return DEC_OK;
    }

    logd("channels:%d,error:%d,header_type:%d,"\
         "num_back_channels:%d,num_front_channels:%d,num_lfe_channels:%d,num_side_channels:%d,"\
         "object_type:%d,ps:%d,samplerate:%lu,samples:%lu,sbr:%d,"\
         "bytesconsumed:%lu,packet_size:%d,channel_position[0]:%d,channel_position[1]:%d\n"
         ,frame_info.channels,frame_info.error,frame_info.header_type
         ,frame_info.num_back_channels,frame_info.num_front_channels,frame_info.num_lfe_channels,frame_info.num_side_channels
         ,frame_info.object_type,frame_info.ps,frame_info.samplerate,frame_info.samples,frame_info.sbr
         ,frame_info.bytesconsumed,aac_decoder->curr_packet->size,frame_info.channel_position[0],frame_info.channel_position[1]);

    audio_pm_enqueue_empty_packet(aac_decoder->decoder.pm, aac_decoder->curr_packet);

    aac_decoder->channels = frame_info.channels;
    aac_decoder->sample_rate = frame_info.samplerate;
    aac_decoder->samples = frame_info.samples;
    aac_decoder->bits_per_sample = 16; //fixed
    //pcm_data_size = aac_decoder->channels *  aac_decoder->samples * aac_decoder->bits_per_sample/8;
    pcm_data_size = aac_decoder->samples * aac_decoder->bits_per_sample / 8;
    if (aac_decoder->decoder.fm == NULL) {
        struct audio_frame_manager_cfg cfg;
        cfg.bits_per_sample = aac_decoder->bits_per_sample;
        //cfg.samples_per_frame = aac_decoder->channels*aac_decoder->samples;
        cfg.samples_per_frame = aac_decoder->samples;
        cfg.frame_count = aac_decoder->frame_count;
        aac_decoder->decoder.fm = audio_fm_create(&cfg);
        if (aac_decoder->decoder.fm == NULL) {
            loge("audio_fm_create fail!!!\n");
            return DEC_ERR_NULL_PTR;
        }
    }

    frame = audio_fm_decoder_get_frame(aac_decoder->decoder.fm);
    if (frame->size < pcm_data_size) {
        if (frame->data) {
            logd("frame->data realloc!!\n");
            mpp_free(frame->data);
            frame->data = NULL;
            frame->data = mpp_alloc(pcm_data_size);
            if (frame->data == NULL){
                loge("mpp_alloc frame->data fail!!!\n");
                return DEC_ERR_NULL_PTR;
            }
            frame->size = pcm_data_size;
        }
    }
    frame->channels = aac_decoder->channels;
    frame->sample_rate = aac_decoder->sample_rate;
    frame->pts = aac_decoder->curr_packet->pts;
    frame->bits_per_sample = aac_decoder->bits_per_sample;
    frame->id = aac_decoder->frame_id++;
    memcpy(frame->data, pcm_data, pcm_data_size);
    if (aac_decoder->curr_packet->flag & PACKET_FLAG_EOS) {
        frame->flag |= PACKET_FLAG_EOS;
        logd("aac_decoder last packet!!!!\n");
    }

    logd("frame:bits_per_sample:%d，channels:%d,flag:0x%x,"\
         "pts:%" PRId64 ",sample_rate:%d,frame_id:%d,size:%d\n",
         frame->bits_per_sample,frame->channels,frame->flag
         ,frame->pts,frame->sample_rate,frame->id,frame->size);

    if (audio_fm_decoder_put_frame(aac_decoder->decoder.fm, frame) != 0) {
        loge("plese check code,why!!!\n");
        return DEC_ERR_NULL_PTR;
    }
    return DEC_OK;
}

int __aac_decode_control(struct aic_audio_decoder *decoder, int cmd, void *param)
{
    return 0;
}
int __aac_decode_reset(struct aic_audio_decoder *decoder)
{
    struct aac_audio_decoder *aac_decoder = (struct aac_audio_decoder *)decoder;
    audio_pm_reset(aac_decoder->decoder.pm);
    audio_fm_reset(aac_decoder->decoder.fm);
    return 0;
}

struct aic_audio_decoder_ops aac_decoder = {
    .name = "aac",
    .init = __aac_decode_init,
    .destroy = __aac_decode_destroy,
    .decode = __aac_decode_frame,
    .control = __aac_decode_control,
    .reset = __aac_decode_reset,
};

struct aic_audio_decoder *create_aac_decoder()
{
    struct aac_audio_decoder *s = (struct aac_audio_decoder *)mpp_alloc(sizeof(struct aac_audio_decoder));
    if (s == NULL) {
        loge("mpp_alloc error!!!!\n");
        return NULL;
    }
    memset(s, 0, sizeof(struct aac_audio_decoder));
    s->decoder.ops = &aac_decoder;
    return &s->decoder;
}
