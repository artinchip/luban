/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: middle media demuxer component
 */

#include "mm_demuxer_component.h"

#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "mpp_log.h"
#include "mpp_list.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "aic_parser.h"
#include "aic_stream.h"
#include "mpp_decoder.h"
#include "mpp_dec_type.h"

#define DEMUX_SKIP_AUDIO_TRACK 0x01
#define DEMUX_SKIP_VIDEO_TRACK 0x02

typedef struct mm_demuxer_data {
    MM_STATE_TYPE state;
    pthread_mutex_t state_lock;
    mm_callback *p_callback;
    void *p_app_data;
    mm_handle h_self;
    mm_port_param port_param;

    mm_param_port_def in_port_def;
    mm_param_port_def out_port_def[2];

    mm_bind_info in_port_bind;
    mm_bind_info out_port_bind[2];
    mm_param_content_uri *p_contenturi;

    s32 eos;
    s32 active_stream_index[2];
    mm_param_u32 stream_num[2];
    mm_audio_param_port_format audio_stream[1];
    mm_video_param_port_format video_stream[1];

    struct aic_parser_av_media_info s_media_info;
    struct aic_parser_packet extra_video_pkt;
    struct aic_parser_packet extra_audio_pkt;
    MM_BOOL extra_video_pkt_flag;
    MM_BOOL extra_audio_pkt_flag;

    pthread_t thread_id;
    struct aic_message_queue s_msg;
    struct aic_parser *p_parser;

    u32 video_pkt_num;
    u32 audio_pkt_num;
    u32 get_video_pkt_ok_num;
    u32 put_video_pkt_ok_num;
    u32 put_video_pkt_fail_num;
    u32 get_audio_pkt_ok_num;
    u32 put_audio_pkt_ok_num;
    u32 put_audio_pkt_fail_num;

    s32 seek_flag;
    s32 need_peek;
    s32 skip_track;
    MM_BOOL net_stream;
} mm_demuxer_data;

static void *mm_demuxer_component_thread(void *p_thread_data);

static s32 mm_demuxer_send_command(mm_handle h_component, MM_COMMAND_TYPE cmd,
                                   u32 param, void *p_cmd_data)
{
    mm_demuxer_data *p_demuxer_data;
    s32 error = MM_ERROR_NONE;
    struct aic_message s_msg;
    memset(&s_msg, 0x00, sizeof(struct aic_message));
    p_demuxer_data =
        (mm_demuxer_data *)(((mm_component *)h_component)->p_comp_private);
    s_msg.message_id = cmd;
    s_msg.param = param;
    s_msg.data_size = 0;

    // now not use always NULL
    if (p_cmd_data != NULL) {
        s_msg.data = p_cmd_data;
        s_msg.data_size = strlen((char *)p_cmd_data);
    }

    aic_msg_put(&p_demuxer_data->s_msg, &s_msg);
    return error;
}

static s32 mm_demuxer_get_parameter(mm_handle h_component, MM_INDEX_TYPE index,
                                    void *p_param)
{
    mm_demuxer_data *p_demuxer_data;
    s32 error = MM_ERROR_NONE;
    s32 tmp1, tmp2;
    mm_param_port_def *p_audio_port, *p_video_port;
    mm_param_u32 *p_aud_stream_num, *p_vid_stream_num;
    s32 *p_aud_stream_idx, *p_vid_stream_idx;

    p_demuxer_data =
        (mm_demuxer_data *)(((mm_component *)h_component)->p_comp_private);
    p_audio_port = &p_demuxer_data->out_port_def[DEMUX_PORT_AUDIO_INDEX];
    p_video_port = &p_demuxer_data->out_port_def[DEMUX_PORT_VIDEO_INDEX];
    p_aud_stream_num = &p_demuxer_data->stream_num[DEMUX_PORT_AUDIO_INDEX];
    p_vid_stream_num = &p_demuxer_data->stream_num[DEMUX_PORT_VIDEO_INDEX];

    p_aud_stream_idx =
        &p_demuxer_data->active_stream_index[DEMUX_PORT_AUDIO_INDEX];
    p_vid_stream_idx =
        &p_demuxer_data->active_stream_index[DEMUX_PORT_VIDEO_INDEX];

    switch (index) {
    case MM_INDEX_CONFIG_TIME_POSITION:
        break;
    case MM_INDEX_CONFIG_TIME_SEEK_MODE:
        break;
    case MM_INDEX_PARAM_CONTENT_URI:
        memcpy(p_param, &p_demuxer_data->p_contenturi,
               ((mm_param_content_uri *)p_param)->size);
        break;
    case MM_INDEX_PARAM_PORT_DEFINITION: { // mm_bind_info
        mm_param_port_def *port = (mm_param_port_def *)p_param;
        if (port->port_index == DEMUX_PORT_AUDIO_INDEX) {
            memcpy(port, p_audio_port, sizeof(mm_param_port_def));
        } else if (port->port_index == DEMUX_PORT_VIDEO_INDEX) {
            memcpy(port, p_video_port, sizeof(mm_param_port_def));
        } else if (port->port_index == DEMUX_PORT_CLOCK_INDEX) {
            memcpy(port, &p_demuxer_data->in_port_def,
                   sizeof(mm_param_port_def));
        } else {
            error = MM_ERROR_BAD_PARAMETER;
        }
        break;
    }
    case MM_INDEX_PARAM_NUM_AVAILABLE_STREAM: // u32
        tmp1 = ((mm_param_u32 *)p_param)->port_index;
        if (tmp1 == DEMUX_PORT_AUDIO_INDEX) {
            ((mm_param_u32 *)p_param)->u32 = p_aud_stream_num->u32;
        } else if (tmp1 == DEMUX_PORT_VIDEO_INDEX) {
            ((mm_param_u32 *)p_param)->u32 = p_vid_stream_num->u32;
        } else {
            error = MM_ERROR_BAD_PARAMETER;
        }
        break;
    case MM_INDEX_PARAM_ACTIVE_STREAM: // u32
        tmp1 = ((mm_param_u32 *)p_param)->port_index;
        tmp2 = ((mm_param_u32 *)p_param)->u32; // start from 0
        if (tmp1 == DEMUX_PORT_AUDIO_INDEX) {
            ((mm_param_u32 *)p_param)->u32 = *p_aud_stream_idx;
        } else if (tmp1 == DEMUX_PORT_VIDEO_INDEX) {
            ((mm_param_u32 *)p_param)->u32 = *p_vid_stream_idx;
        } else {
            error = MM_ERROR_BAD_PARAMETER;
        }

        break;
    case MM_INDEX_PARAM_AUDIO_PORT_FORMAT:
        tmp1 = ((mm_audio_param_port_format *)p_param)->port_index;
        tmp2 = ((mm_audio_param_port_format *)p_param)->index;
        if (tmp1 != DEMUX_PORT_AUDIO_INDEX ||
            tmp2 > p_aud_stream_num->u32 - 1) {
            error = MM_ERROR_BAD_PARAMETER;
            break;
        }
        ((mm_audio_param_port_format *)p_param)->encoding =
            p_demuxer_data->audio_stream[tmp2].encoding;
        break;
    case MM_INDEX_PARAM_VIDEO_PORT_FORMAT: // mm_video_param_port_format
        tmp1 = ((mm_video_param_port_format *)p_param)->port_index;
        tmp2 = ((mm_video_param_port_format *)p_param)->index;
        if (tmp1 != DEMUX_PORT_VIDEO_INDEX ||
            tmp2 > (p_vid_stream_num->u32 - 1)) {
            error = MM_ERROR_BAD_PARAMETER;
            break;
        }
        ((mm_video_param_port_format *)p_param)->compression_format =
            p_demuxer_data->video_stream[tmp2].compression_format;
        ((mm_video_param_port_format *)p_param)->color_format =
            p_demuxer_data->video_stream[tmp2].color_format;
        break;

    default:
        break;
    }
    return error;
}

static MM_AUDIO_CODING_TYPE
mm_demuxer_audio_format_trans(enum aic_audio_codec_type audio_type)
{
    MM_AUDIO_CODING_TYPE ret = MM_AUDIO_CODING_UNUSED;
    if (audio_type == MPP_CODEC_AUDIO_DECODER_MP3) {
        ret = MM_AUDIO_CODING_MP3;
    } else if (audio_type == MPP_CODEC_AUDIO_DECODER_AAC) {
        ret = MM_AUDIO_CODING_AAC;
    } else {
        loge("unsp_port codec!!!\n");
        ret = MM_AUDIO_CODING_MAX;
    }

    return ret;
}

static MM_VIDEO_CODING_TYPE
mm_demuxer_video_format_trans(enum mpp_codec_type nVideoType)
{
    MM_VIDEO_CODING_TYPE ret = MM_VIDEO_CODING_UNUSED;
    if (nVideoType == MPP_CODEC_VIDEO_DECODER_H264) {
        ret = MM_VIDEO_CODING_AVC;
    } else if (nVideoType == MPP_CODEC_VIDEO_DECODER_MJPEG) {
        ret = MM_VIDEO_CODING_MJPEG;
    } else {
        loge("unsp_port codec!!!\n");
        ret = MM_VIDEO_CODING_MAX;
    }
    return ret;
}

static void mm_demuxer_event_notify(mm_demuxer_data *p_demuxer_data,
                                    MM_EVENT_TYPE event, u32 data1, u32 data2,
                                    void *p_event_data)
{
    if (p_demuxer_data && p_demuxer_data->p_callback &&
        p_demuxer_data->p_callback->event_handler) {
        p_demuxer_data->p_callback->event_handler(p_demuxer_data->h_self,
                                                  p_demuxer_data->p_app_data,
                                                  event, data1, data2,
                                                  p_event_data);
    }
}

static s32 mm_demuxer_index_param_contenturi(mm_demuxer_data *p_demuxer_data,
                                             mm_param_content_uri *p_contenturi)
{
    int ret = 0;
    MM_BOOL b_audio_find = MM_FALSE;
    MM_BOOL b_video_find = MM_FALSE;
    mm_param_port_def *p_audio_port, *p_video_port;
    mm_param_u32 *p_aud_stream_num, *p_vid_stream_num;
    s32 *p_aud_stream_idx, *p_vid_stream_idx;

    p_audio_port = &p_demuxer_data->out_port_def[DEMUX_PORT_AUDIO_INDEX];
    p_video_port = &p_demuxer_data->out_port_def[DEMUX_PORT_VIDEO_INDEX];
    p_aud_stream_num = &p_demuxer_data->stream_num[DEMUX_PORT_AUDIO_INDEX];
    p_vid_stream_num = &p_demuxer_data->stream_num[DEMUX_PORT_VIDEO_INDEX];
    p_aud_stream_idx =
        &p_demuxer_data->active_stream_index[DEMUX_PORT_AUDIO_INDEX];
    p_vid_stream_idx =
        &p_demuxer_data->active_stream_index[DEMUX_PORT_VIDEO_INDEX];

    if (p_demuxer_data->p_contenturi == NULL) {
        p_demuxer_data->p_contenturi = (mm_param_content_uri *)mpp_alloc(
            sizeof(mm_param_content_uri) + MM_MAX_STRINGNAME_SIZE);
        if (p_demuxer_data->p_contenturi == NULL) {
            loge("alloc for content uri failed\n");
            return MM_ERROR_FORMAT_NOT_DETECTED;
        }
    }
    memcpy(p_demuxer_data->p_contenturi, p_contenturi, p_contenturi->size);
    if (p_demuxer_data->p_parser) {
        aic_parser_destroy(p_demuxer_data->p_parser);
        p_demuxer_data->p_parser = NULL;
        p_demuxer_data->eos = 0;
        p_demuxer_data->need_peek = 1;
    }
    ret = aic_parser_create(p_demuxer_data->p_contenturi->content_uri,
                            &p_demuxer_data->p_parser);
    printf("[%s:%d]p_parser=%p,content_uri:%s\n", __FUNCTION__, __LINE__,
           p_demuxer_data->p_parser, p_demuxer_data->p_contenturi->content_uri);
    if (NULL == p_demuxer_data->p_parser) { /*create parser fail*/
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                MM_ERROR_FORMAT_NOT_DETECTED,
                                p_demuxer_data->state, NULL);
        loge("MM_ERROR_FORMAT_NOT_DETECTED\n");
        return MM_ERROR_FORMAT_NOT_DETECTED;
    }

    /*******************************************************************************
        Here,it will takes a lot of time,the larger the file,the longer it takes.
        so,if you want to optimize it,please optimize parser.
    *******************************************************************************/
    time_start(aic_parser_init);
    ret = aic_parser_init(p_demuxer_data->p_parser);
    if (0 != ret) {
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                MM_ERROR_FORMAT_NOT_DETECTED,
                                p_demuxer_data->state, NULL);
        aic_parser_destroy(p_demuxer_data->p_parser);
        p_demuxer_data->p_parser = NULL;
        return MM_ERROR_FORMAT_NOT_DETECTED;
    }
    time_end(aic_parser_init);
    memset(&p_demuxer_data->s_media_info, 0x00,
           sizeof(struct aic_parser_av_media_info));
    ret = aic_parser_get_media_info(p_demuxer_data->p_parser,
                                    &p_demuxer_data->s_media_info);
    if (0 != ret) { /*get_media_info fail*/
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                MM_ERROR_FORMAT_NOT_DETECTED,
                                p_demuxer_data->state, NULL);
        loge("MM_ERROR_FORMAT_NOT_DETECTED\n");
        aic_parser_destroy(p_demuxer_data->p_parser);
        p_demuxer_data->p_parser = NULL;
        return MM_ERROR_FORMAT_NOT_DETECTED;
    }
    // set port info
    if (p_demuxer_data->s_media_info.has_audio) {
        p_demuxer_data->audio_stream[0].encoding =
            mm_demuxer_audio_format_trans(
                p_demuxer_data->s_media_info.audio_stream.codec_type);
        p_aud_stream_num->u32 = 1;
        p_audio_port->format.audio.encoding =
            p_demuxer_data->audio_stream[0].encoding;
        *p_aud_stream_idx = 0;
        b_audio_find = MM_TRUE;
    } else {
        p_aud_stream_num->u32 = 0;
    }
    if (p_demuxer_data->s_media_info.has_video) {
        p_demuxer_data->video_stream[0].compression_format =
            mm_demuxer_video_format_trans(
                p_demuxer_data->s_media_info.video_stream.codec_type);
        p_vid_stream_num->u32 = 1;
        p_video_port->format.video.compression_format =
            p_demuxer_data->video_stream[0].compression_format;
        *p_vid_stream_idx = 0;
        b_video_find = MM_TRUE;
    } else {
        p_vid_stream_num->u32 = 0;
    }

    p_demuxer_data->audio_pkt_num = 0;
    p_demuxer_data->get_audio_pkt_ok_num = 0;
    p_demuxer_data->put_audio_pkt_ok_num = 0;
    p_demuxer_data->put_audio_pkt_fail_num = 0;
    p_demuxer_data->video_pkt_num = 0;
    p_demuxer_data->get_video_pkt_ok_num = 0;
    p_demuxer_data->put_video_pkt_ok_num = 0;
    p_demuxer_data->put_video_pkt_fail_num = 0;

    if (b_audio_find || b_video_find) {
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_PORT_FORMAT_DETECTED,
                                0, 0, &p_demuxer_data->s_media_info);
        printf("[%s:%d]MM_EVENT_PORT_FORMAT_DETECTED\n", __FUNCTION__,
               __LINE__);

        if (b_audio_find &&
            p_demuxer_data->s_media_info.audio_stream.extra_data_size > 0 &&
            p_demuxer_data->s_media_info.audio_stream.extra_data != NULL) {
            memset(&p_demuxer_data->extra_audio_pkt, 0x00,
                   sizeof(struct aic_parser_packet));
            logi("audio_stream extra_data_size:%d,extra_data:%p\n",
                 p_demuxer_data->s_media_info.audio_stream.extra_data_size,
                 p_demuxer_data->s_media_info.audio_stream.extra_data);
            p_demuxer_data->extra_audio_pkt.size =
                p_demuxer_data->s_media_info.audio_stream.extra_data_size;
            p_demuxer_data->extra_audio_pkt.flag |= PACKET_FLAG_EXTRA_DATA;

            p_demuxer_data->extra_audio_pkt.data =
                mpp_alloc(p_demuxer_data->extra_audio_pkt.size);
            memcpy(p_demuxer_data->extra_audio_pkt.data,
                   p_demuxer_data->s_media_info.audio_stream.extra_data,
                   p_demuxer_data->s_media_info.audio_stream.extra_data_size);
            p_demuxer_data->extra_audio_pkt.type = MPP_MEDIA_TYPE_AUDIO;
            p_demuxer_data->extra_audio_pkt_flag = MM_TRUE;
            p_demuxer_data->audio_pkt_num++;
        }
        logi("video_stream extra_data_size:%d,extra_data:%p\n",
             p_demuxer_data->s_media_info.video_stream.extra_data_size,
             p_demuxer_data->s_media_info.video_stream.extra_data);

        if (b_video_find &&
            p_demuxer_data->s_media_info.video_stream.extra_data_size > 0 &&
            p_demuxer_data->s_media_info.video_stream.extra_data != NULL) {

            memset(&p_demuxer_data->extra_video_pkt, 0x00,
                   sizeof(struct aic_parser_packet));
            p_demuxer_data->extra_video_pkt.size =
                p_demuxer_data->s_media_info.video_stream.extra_data_size;
            p_demuxer_data->extra_video_pkt.flag |= PACKET_FLAG_EXTRA_DATA;
            logi("s_pkt.flag:0x%x\n", p_demuxer_data->extra_video_pkt.flag);

            p_demuxer_data->extra_video_pkt.data =
                mpp_alloc(p_demuxer_data->extra_video_pkt.size);

            memcpy(p_demuxer_data->extra_video_pkt.data,
                   p_demuxer_data->s_media_info.video_stream.extra_data,
                   p_demuxer_data->s_media_info.video_stream.extra_data_size);
            p_demuxer_data->extra_video_pkt.type = MPP_MEDIA_TYPE_VIDEO;
            p_demuxer_data->extra_video_pkt_flag = MM_TRUE;
            p_demuxer_data->video_pkt_num++;
#if 0
            int i = 0;
            printf(
                "-----------------------------extra_data,size:%d------------------------------------\n",
                p_demuxer_data->s_media_info.video_stream.extra_data_size);
            for (i = 0;
                 i < p_demuxer_data->s_media_info.video_stream.extra_data_size;
                 i++) {
                printf("%02x ",
                       p_demuxer_data->s_media_info.video_stream.extra_data[i]);
            }
            printf(
                "\n----------------------------------------------------------------------------\n");
#endif
        }
    } else {
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                MM_ERROR_FORMAT_NOT_DETECTED, 0, NULL);
        loge("MM_ERROR_FORMAT_NOT_DETECTED\n");
        aic_parser_destroy(p_demuxer_data->p_parser);
        p_demuxer_data->p_parser = NULL;
        return MM_ERROR_FORMAT_NOT_DETECTED;
    }
    p_demuxer_data->net_stream = 0;
    if (!strncmp((char *)p_demuxer_data->p_contenturi->content_uri,"rtsp://",7)) {
        p_demuxer_data->net_stream = 1;
    }
    return MM_EVENT_PORT_FORMAT_DETECTED;
}

static s32 mm_demuxer_set_parameter(mm_handle h_component, MM_INDEX_TYPE index,
                                    void *p_param)
{
    mm_demuxer_data *p_demuxer_data;
    s32 error = MM_ERROR_NONE;
    s32 tmp1, tmp2;
    mm_param_port_def *p_audio_port, *p_video_port;
    s32 *p_aud_stream_idx, *p_vid_stream_idx;
    mm_param_u32 *p_aud_stream_num, *p_vid_stream_num;
    p_demuxer_data =
        (mm_demuxer_data *)(((mm_component *)h_component)->p_comp_private);
    p_audio_port = &p_demuxer_data->out_port_def[DEMUX_PORT_AUDIO_INDEX];
    p_video_port = &p_demuxer_data->out_port_def[DEMUX_PORT_VIDEO_INDEX];
    p_aud_stream_num = &p_demuxer_data->stream_num[DEMUX_PORT_AUDIO_INDEX];
    p_vid_stream_num = &p_demuxer_data->stream_num[DEMUX_PORT_VIDEO_INDEX];
    p_aud_stream_idx =
        &p_demuxer_data->active_stream_index[DEMUX_PORT_AUDIO_INDEX];
    p_vid_stream_idx =
        &p_demuxer_data->active_stream_index[DEMUX_PORT_VIDEO_INDEX];

    switch (index) {
    case MM_INDEX_CONFIG_TIME_POSITION:
        break;
    case MM_INDEX_CONFIG_TIME_SEEK_MODE:
        break;
    case MM_INDEX_PARAM_CONTENT_URI:
        mm_demuxer_index_param_contenturi(p_demuxer_data,
                                          (mm_param_content_uri *)p_param);
        break;
    case MM_INDEX_PARAM_PORT_DEFINITION:
        break;
    case MM_INDEX_PARAM_NUM_AVAILABLE_STREAM: // u32
        break;
    case MM_INDEX_PARAM_ACTIVE_STREAM: // u32
        tmp1 = ((mm_param_u32 *)p_param)->port_index;
        tmp2 = ((mm_param_u32 *)p_param)->u32; // start from 0
        if (tmp1 == DEMUX_PORT_AUDIO_INDEX) {
            if (tmp2 > p_aud_stream_num->u32 - 1)
                tmp2 = 0;
            p_audio_port->format.audio.encoding =
                p_demuxer_data->audio_stream[tmp2].encoding;
            *p_aud_stream_idx = tmp2;
        } else if (tmp1 == DEMUX_PORT_VIDEO_INDEX) {
            if (tmp2 > p_vid_stream_num->u32 - 1)
                tmp2 = 0;
            p_video_port->format.video.compression_format =
                p_demuxer_data->video_stream[tmp2].compression_format;
            p_video_port->format.video.color_format =
                p_demuxer_data->video_stream[tmp2].color_format;
            *p_vid_stream_idx = tmp2;
        } else {
            error = MM_ERROR_BAD_PARAMETER;
        }
        break;

    case MM_INDEX_VENDOR_DEMUXER_SKIP_TRACK:
        tmp1 = ((mm_param_skip_track*)p_param)->port_index;
        if (tmp1 == DEMUX_PORT_AUDIO_INDEX) {
            p_demuxer_data->skip_track |= DEMUX_SKIP_AUDIO_TRACK;
            aic_parser_control(p_demuxer_data->p_parser, PARSER_AUDIO_SKIP_PACKET, NULL);
        } else if (tmp1 == DEMUX_PORT_VIDEO_INDEX) {
            p_demuxer_data->skip_track |= DEMUX_SKIP_VIDEO_TRACK;
            aic_parser_control(p_demuxer_data->p_parser, PARSER_VIDEO_SKIP_PACKET, NULL);
        }
        break;
    default:
        break;
    }
    return error;
}

static s32 mm_demuxer_get_config(mm_handle h_component, MM_INDEX_TYPE index,
                                 void *p_config)
{
    s32 error = MM_ERROR_NONE;

    switch (index) {
    case MM_INDEX_CONFIG_TIME_POSITION:
        break;
    case MM_INDEX_CONFIG_TIME_SEEK_MODE:
        break;
    default:
        break;
    }
    return error;
}

static s32 mm_demuxer_set_config(mm_handle h_component, MM_INDEX_TYPE index,
                                 void *p_config)
{
    s32 error = MM_ERROR_NONE;
    mm_demuxer_data *p_demuxer_data =
        (mm_demuxer_data *)(((mm_component *)h_component)->p_comp_private);
    switch ((s32)index) {
    case MM_INDEX_CONFIG_TIME_POSITION: // do seek
    {
        // 1 when seeking ,stop peek/read
        // 2 async send command ,sync here do seek.
        // 3 when demux in seeking .other adec and vdec how to do
        int ret = 0;
        mm_time_config_timestamp *timestamp =
            (mm_time_config_timestamp *)p_config;
        logd("timestamp.timestamp:" FMT_d64 "\n", timestamp->timestamp);
        // 1  seek
        ret =
            aic_parser_seek(p_demuxer_data->p_parser, timestamp->timestamp);
        if (ret == 0) {
            error = MM_ERROR_NONE;
            p_demuxer_data->need_peek = 1;
        } else {
            error = MM_ERROR_UNDEFINED;
        }
        break;
    }
    case MM_INDEX_CONFIG_TIME_SEEK_MODE:
        break;
    case MM_INDEX_VENDOR_CLEAR_BUFFER:
        p_demuxer_data->eos = 0;
        break;
    default:
        break;
    }
    return error;
}

static s32 mm_demuxer_get_state(mm_handle h_component, MM_STATE_TYPE *p_state)
{
    mm_demuxer_data *p_demuxer_data;
    s32 error = MM_ERROR_NONE;
    p_demuxer_data =
        (mm_demuxer_data *)(((mm_component *)h_component)->p_comp_private);
    pthread_mutex_lock(&p_demuxer_data->state_lock);
    *p_state = p_demuxer_data->state;
    pthread_mutex_unlock(&p_demuxer_data->state_lock);
    return error;
}

static s32 mm_demuxer_bind_request(mm_handle h_comp, u32 port,
                                   mm_handle h_bind_comp, u32 bind_port)
{
    s32 error = MM_ERROR_NONE;
    mm_param_port_def *p_port;
    mm_bind_info *p_bind_info;
    mm_demuxer_data *p_demuxer_data;
    p_demuxer_data =
        (mm_demuxer_data *)(((mm_component *)h_comp)->p_comp_private);
    if (p_demuxer_data->state != MM_STATE_LOADED &&
        p_demuxer_data->state != MM_STATE_IDLE) {
        loge(
            "Component is not in MM_STATE_LOADED,it is in%d,it can not tunnel\n",
            p_demuxer_data->state);
        return MM_ERROR_INVALID_STATE;
    }

    if (port == DEMUX_PORT_AUDIO_INDEX) {
        p_port = &p_demuxer_data->out_port_def[DEMUX_PORT_AUDIO_INDEX];
        p_bind_info = &p_demuxer_data->out_port_bind[DEMUX_PORT_AUDIO_INDEX];
    } else if (port == DEMUX_PORT_VIDEO_INDEX) {
        p_port = &p_demuxer_data->out_port_def[DEMUX_PORT_VIDEO_INDEX];
        p_bind_info = &p_demuxer_data->out_port_bind[DEMUX_PORT_VIDEO_INDEX];
    } else if (port == DEMUX_PORT_CLOCK_INDEX) {
        p_port = &p_demuxer_data->in_port_def;
        p_bind_info = &p_demuxer_data->in_port_bind;
    } else {
        loge("component can not find port:%u\n", port);
        return MM_ERROR_BAD_PARAMETER;
    }

    // cancle setup tunnel
    if (NULL == h_bind_comp && 0 == bind_port) {
        p_bind_info->flag = MM_FALSE;
        p_bind_info->bind_port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_comp;
        return MM_ERROR_NONE;
    }

    if (p_port->dir == MM_DIR_OUTPUT) {
        p_bind_info->bind_port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_comp;
        p_bind_info->flag = MM_TRUE;
    } else if (p_port->dir == MM_DIR_INPUT) {
        mm_param_port_def bind_port_param;

        bind_port_param.port_index = bind_port;

        mm_get_parameter(h_bind_comp, MM_INDEX_PARAM_PORT_DEFINITION,
                         &bind_port_param);

        if (bind_port_param.dir != MM_DIR_OUTPUT) {
            loge("both ports are input.\n");
            return MM_ERROR_PORT_NOT_COMPATIBLE;
        }

        p_bind_info->bind_port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_comp;
        p_bind_info->flag = MM_TRUE;
    } else {
        loge("port is neither output nor input.\n");
        return MM_ERROR_PORT_NOT_COMPATIBLE;
    }
    return error;
}

static s32 mm_demuxer_set_callback(mm_handle h_component, mm_callback *p_cb,
                                   void *p_app_data)
{
    s32 error = MM_ERROR_NONE;
    mm_demuxer_data *p_demuxer_data;
    p_demuxer_data =
        (mm_demuxer_data *)(((mm_component *)h_component)->p_comp_private);
    p_demuxer_data->p_callback = p_cb;
    p_demuxer_data->p_app_data = p_app_data;
    return error;
}

s32 mm_demuxer_component_deinit(mm_handle h_component)
{
    s32 error = MM_ERROR_NONE;
    mm_component *p_comp;
    mm_demuxer_data *p_demuxer_data;
    p_comp = (mm_component *)h_component;
    struct aic_message s_msg;

    p_demuxer_data = (mm_demuxer_data *)p_comp->p_comp_private;
    pthread_mutex_lock(&p_demuxer_data->state_lock);
    if (p_demuxer_data->state != MM_STATE_LOADED) {
        logd(
            "compoent is in %d,but not in MM_STATE_LOADED(1),can ont FreeHandle.\n",
            p_demuxer_data->state);
        pthread_mutex_unlock(&p_demuxer_data->state_lock);
        return MM_ERROR_INVALID_STATE;
    }
    pthread_mutex_unlock(&p_demuxer_data->state_lock);

    s_msg.message_id = MM_COMMAND_STOP;
    s_msg.data_size = 0;
    aic_msg_put(&p_demuxer_data->s_msg, &s_msg);
    pthread_join(p_demuxer_data->thread_id, (void *)&error);

    pthread_mutex_destroy(&p_demuxer_data->state_lock);

    aic_msg_destroy(&p_demuxer_data->s_msg);

    if (p_demuxer_data->p_parser) {
        aic_parser_destroy(p_demuxer_data->p_parser);
        p_demuxer_data->p_parser = NULL;
    }

    if (p_demuxer_data->p_contenturi) {
        mpp_free(p_demuxer_data->p_contenturi);
        p_demuxer_data->p_contenturi = NULL;
    }

    mpp_free(p_demuxer_data);
    p_demuxer_data = NULL;

    logd("mm_demuxer_component_deinit\n");

    return error;
}

s32 mm_demuxer_component_init(mm_handle h_component)
{
    mm_component *p_comp;
    mm_demuxer_data *p_demuxer_data;
    s32 error = MM_ERROR_NONE;
    u32 err;

    mm_param_port_def *p_audio_port, *p_video_port;
    mm_param_u32 *p_aud_stream_num, *p_vid_stream_num;

    MM_BOOL b_msg_creat = MM_FALSE;
    MM_BOOL b_state_lock_init = MM_FALSE;

    logd("mm_demuxer_ComponentInit....");

    p_comp = (mm_component *)h_component;

    p_demuxer_data = (mm_demuxer_data *)mpp_alloc(sizeof(mm_demuxer_data));

    if (NULL == p_demuxer_data) {
        loge("mpp_alloc(sizeof(mm_demuxer_data) fail!\n");
        return MM_ERROR_INSUFFICIENT_RESOURCES;
    }

    memset(p_demuxer_data, 0x0, sizeof(mm_demuxer_data));

    p_comp->p_comp_private = (void *)p_demuxer_data;
    p_demuxer_data->state = MM_STATE_LOADED;
    p_demuxer_data->h_self = p_comp;

    p_comp->set_callback = mm_demuxer_set_callback;
    p_comp->send_command = mm_demuxer_send_command;
    p_comp->get_state = mm_demuxer_get_state;
    p_comp->get_parameter = mm_demuxer_get_parameter;
    p_comp->set_parameter = mm_demuxer_set_parameter;
    p_comp->get_config = mm_demuxer_get_config;
    p_comp->set_config = mm_demuxer_set_config;
    p_comp->bind_request = mm_demuxer_bind_request;
    p_comp->deinit = mm_demuxer_component_deinit;

    p_demuxer_data->port_param.ports = 3;
    p_demuxer_data->port_param.start_port_num = 0x0;
    p_audio_port = &p_demuxer_data->out_port_def[DEMUX_PORT_AUDIO_INDEX];
    p_video_port = &p_demuxer_data->out_port_def[DEMUX_PORT_VIDEO_INDEX];
    p_aud_stream_num = &p_demuxer_data->stream_num[DEMUX_PORT_AUDIO_INDEX];
    p_vid_stream_num = &p_demuxer_data->stream_num[DEMUX_PORT_VIDEO_INDEX];

    p_audio_port->port_index = DEMUX_PORT_AUDIO_INDEX;
    p_audio_port->enable = MM_TRUE;
    p_audio_port->dir = MM_DIR_OUTPUT;
    p_video_port->port_index = DEMUX_PORT_VIDEO_INDEX;
    p_video_port->enable = MM_TRUE;
    p_video_port->dir = MM_DIR_OUTPUT;
    p_demuxer_data->in_port_def.port_index = DEMUX_PORT_CLOCK_INDEX;
    p_demuxer_data->in_port_def.enable = MM_TRUE;
    p_demuxer_data->in_port_def.dir = MM_DIR_INPUT;

    p_demuxer_data->extra_video_pkt.data = NULL;
    p_demuxer_data->extra_video_pkt.size = 0;
    p_demuxer_data->extra_audio_pkt.data = NULL;
    p_demuxer_data->extra_audio_pkt.size = 0;
    p_demuxer_data->extra_video_pkt_flag = MM_FALSE;
    p_demuxer_data->extra_audio_pkt_flag = MM_FALSE;

    p_aud_stream_num->port_index = DEMUX_PORT_AUDIO_INDEX;
    p_vid_stream_num->port_index = DEMUX_PORT_VIDEO_INDEX;

    p_demuxer_data->eos = 0;

    if (aic_msg_create(&p_demuxer_data->s_msg) < 0) {
        loge("aic_msg_create fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    b_msg_creat = MM_TRUE;

    if (pthread_mutex_init(&p_demuxer_data->state_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    b_state_lock_init = MM_TRUE;

    // Create the component thread
    err = pthread_create(&p_demuxer_data->thread_id, NULL,
                         mm_demuxer_component_thread, p_demuxer_data);
    if (err) {
        loge("pthread_create fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }

    return error;

_EXIT:

    if (b_state_lock_init) {
        pthread_mutex_destroy(&p_demuxer_data->state_lock);
    }
    if (b_msg_creat) {
        aic_msg_destroy(&p_demuxer_data->s_msg);
    }

    if (p_demuxer_data) {
        mpp_free(p_demuxer_data);
        p_demuxer_data = NULL;
    }

    return error;
}

static void mm_demuxer_state_change_to_invalid(mm_demuxer_data *p_demuxer_data)
{
    p_demuxer_data->state = MM_STATE_INVALID;
    mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                            MM_ERROR_INVALID_STATE, 0, NULL);
    mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_CMD_COMPLETE,
                            MM_COMMAND_STATE_SET, p_demuxer_data->state, NULL);
}

static void mm_demuxer_state_change_to_idle(mm_demuxer_data *p_demuxer_data)
{
    if ((MM_STATE_LOADED != p_demuxer_data->state) &&
        (MM_STATE_EXECUTING != p_demuxer_data->state) &&
        (MM_STATE_PAUSE != p_demuxer_data->state)) {
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                MM_ERROR_INCORRECT_STATE_TRANSITION,
                                p_demuxer_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_demuxer_data->state = MM_STATE_IDLE;
    mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_CMD_COMPLETE,
                            MM_COMMAND_STATE_SET, p_demuxer_data->state, NULL);
}

static void mm_demuxer_state_change_to_loaded(mm_demuxer_data *p_demuxer_data)
{
    if (MM_STATE_IDLE == p_demuxer_data->state) {
        p_demuxer_data->eos = 0;
        p_demuxer_data->skip_track = 0;

        p_demuxer_data->state = MM_STATE_LOADED;
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_CMD_COMPLETE,
                                MM_COMMAND_STATE_SET, p_demuxer_data->state,
                                NULL);
        if (p_demuxer_data->p_parser) {
            aic_parser_destroy(p_demuxer_data->p_parser);
            p_demuxer_data->p_parser = NULL;
            p_demuxer_data->eos = 0;
            p_demuxer_data->need_peek = 1;
        }
        memset(&p_demuxer_data->s_media_info,0x00,
            sizeof(struct aic_parser_av_media_info));
        if (p_demuxer_data->extra_audio_pkt_flag) {
            if (p_demuxer_data->extra_audio_pkt.data)
                mpp_free(p_demuxer_data->extra_audio_pkt.data);
            p_demuxer_data->extra_audio_pkt.data = NULL;
            p_demuxer_data->extra_audio_pkt.size = 0;
            p_demuxer_data->extra_audio_pkt_flag = MM_FALSE;
        }
        if (p_demuxer_data->extra_video_pkt_flag) {
            if (p_demuxer_data->extra_video_pkt.data)
                mpp_free(p_demuxer_data->extra_video_pkt.data);
            p_demuxer_data->extra_video_pkt.data = NULL;
            p_demuxer_data->extra_video_pkt.size = 0;
            p_demuxer_data->extra_video_pkt_flag = MM_FALSE;
        }
    } else {
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                MM_ERROR_INCORRECT_STATE_TRANSITION,
                                p_demuxer_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
    }
}

static void
mm_demuxer_state_change_to_executing(mm_demuxer_data *p_demuxer_data)
{
    if (MM_STATE_IDLE == p_demuxer_data->state) {
        if (NULL == p_demuxer_data->p_parser) {
            mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                    MM_ERROR_INCORRECT_STATE_TRANSITION,
                                    p_demuxer_data->state, NULL);
            loge(
                "p_demuxer_data->p_parser is not created,please set param uri!!!!!\n");
            return;
        }
    } else if (MM_STATE_PAUSE == p_demuxer_data->state) {
        //
    } else {
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                MM_ERROR_INCORRECT_STATE_TRANSITION,
                                p_demuxer_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_demuxer_data->state = MM_STATE_EXECUTING;
}

static void mm_demuxer_state_change_to_pause(mm_demuxer_data *p_demuxer_data)
{
    if (MM_STATE_EXECUTING == p_demuxer_data->state) {
        //
    } else {
        mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                MM_ERROR_INCORRECT_STATE_TRANSITION,
                                p_demuxer_data->state, NULL);
        logd("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_demuxer_data->state = MM_STATE_PAUSE;
}

static int mm_demuxer_component_process_cmd(mm_demuxer_data *p_demuxer_data)
{
    s32 cmd = MM_COMMAND_UNKNOWN;
    s32 cmd_data;
    struct aic_message message;

    if (aic_msg_get(&p_demuxer_data->s_msg, &message) == 0) {
        cmd = message.message_id;
        cmd_data = message.param;
        logi("cmd:%d, cmd_data:%d\n", cmd, cmd_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            pthread_mutex_lock(&p_demuxer_data->state_lock);
            if (p_demuxer_data->state == (MM_STATE_TYPE)(cmd_data)) {
                mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_ERROR,
                                        MM_ERROR_SAME_STATE, 0, NULL);
                pthread_mutex_unlock(&p_demuxer_data->state_lock);
                goto CMD_EXIT;
            }
            switch ((MM_STATE_TYPE)(cmd_data)) {
            case MM_STATE_INVALID:
                mm_demuxer_state_change_to_invalid(p_demuxer_data);
                break;
            case MM_STATE_LOADED:
                mm_demuxer_state_change_to_loaded(p_demuxer_data);
                break;
            case MM_STATE_IDLE:
                mm_demuxer_state_change_to_idle(p_demuxer_data);
                break;
            case MM_STATE_EXECUTING:
                mm_demuxer_state_change_to_executing(p_demuxer_data);
                break;
            case MM_STATE_PAUSE:
                mm_demuxer_state_change_to_pause(p_demuxer_data);
                break;
            default:
                break;
            }
            pthread_mutex_unlock(&p_demuxer_data->state_lock);
        } else if (MM_COMMAND_STOP == cmd) {
            logi("mm_demuxer_component_thread ready to exit!!!\n");
        }
    }

CMD_EXIT:
    return cmd;
}

static int
mm_demuxer_component_process_eos_pkt(mm_demuxer_data *p_demuxer_data)
{
    /*Get video decoder handle*/
    mm_component *h_vdec_comp =
        p_demuxer_data->out_port_bind[DEMUX_PORT_VIDEO_INDEX].p_bind_comp;
    mm_component *h_adec_comp =
        p_demuxer_data->out_port_bind[DEMUX_PORT_AUDIO_INDEX].p_bind_comp;

    /*when the final video or audio packet can't get PACKET_EOS flag,
     *then parser send a empty packet to demuxer , and demuxer need
     *wakup dec comp and send end flag to dec comp
     */
    if (h_vdec_comp != NULL && (p_demuxer_data->skip_track == 0 ||
                                p_demuxer_data->skip_track & DEMUX_SKIP_AUDIO_TRACK)) {
        mm_send_command(h_vdec_comp, MM_COMMAND_EOS, 0, NULL);
    }

    if (h_adec_comp != NULL && (p_demuxer_data->skip_track == 0 ||
                                p_demuxer_data->skip_track & DEMUX_SKIP_VIDEO_TRACK)) {
        mm_send_command(h_adec_comp, MM_COMMAND_EOS, 0, NULL);
    }

    return MM_ERROR_NONE;
}

static int
mm_demuxer_component_process_video_pkt(mm_demuxer_data *p_demuxer_data,
                                       struct aic_parser_packet *p_pkt)
{
    s32 ret = MM_ERROR_NONE;
    long diff = 0;
    struct mpp_packet pkt = {0};
    struct mpp_decoder *p_decoder = NULL;
    struct timespec before = {0}, after = {0};

    /*Get video decoder handle*/
    mm_component *h_vdec_comp =
        p_demuxer_data->out_port_bind[DEMUX_PORT_VIDEO_INDEX].p_bind_comp;

    if (h_vdec_comp == NULL) {
        logd("get h_vdec_comp is null\n");
        /*if viddec format not support, should peek next video packet*/
        mm_component *h_adec_comp =
            p_demuxer_data->out_port_bind[DEMUX_PORT_AUDIO_INDEX].p_bind_comp;
        if (h_adec_comp) {
            p_demuxer_data->need_peek = MM_TRUE;
        }
        return MM_ERROR_NULL_POINTER;
    }

    mm_get_parameter(h_vdec_comp, MM_INDEX_PARAM_VIDEO_DECODER_HANDLE,
                     (void *)&p_decoder);
    if (p_decoder == NULL) {
        logd("get video decoder is null\n");
        return MM_ERROR_NULL_POINTER;
    }

    /*process extra video pkt and put it to decoder*/
    if (p_demuxer_data->extra_video_pkt_flag) {
        pkt.size = p_demuxer_data->extra_video_pkt.size;
        ret = mpp_decoder_get_packet(p_decoder, &pkt, pkt.size);
        if (ret != 0) {
            aic_msg_wait_new_msg(&p_demuxer_data->s_msg, 0);
            return MM_ERROR_EMPTY_DATA;
        }
        p_demuxer_data->get_video_pkt_ok_num++;
        memcpy(pkt.data, p_demuxer_data->extra_video_pkt.data, pkt.size);
        if (p_demuxer_data->net_stream) {
             pkt.flag = 0;
        } else {
             pkt.flag |= p_demuxer_data->extra_video_pkt.flag;
        }
        pkt.pts = p_demuxer_data->extra_video_pkt.pts;
        ret = mpp_decoder_put_packet(p_decoder, &pkt);
        if (ret != 0) {
            loge("put extra pkt to decoder failed %x!!!\n", ret);
            p_demuxer_data->put_video_pkt_fail_num++;
            return ret;
        }
        mm_send_command(h_vdec_comp, MM_COMMAND_WKUP, 0, NULL);
        logi("send video extra data,pts:" FMT_d64 ",type = %d,size:%d,flag:0x%x\n",
             p_pkt->pts, p_pkt->type, p_pkt->size, p_pkt->flag);

        p_demuxer_data->put_video_pkt_ok_num++;
        p_demuxer_data->extra_video_pkt_flag = MM_FALSE;

        if (p_demuxer_data->extra_video_pkt.data) {
            mpp_free(p_demuxer_data->extra_video_pkt.data);
            p_demuxer_data->extra_video_pkt.data = NULL;
            p_demuxer_data->extra_video_pkt.size = 0;
        }
    }

    /*Get empty packet from decoder*/
    pkt.size = p_pkt->size;
    ret = mpp_decoder_get_packet(p_decoder, &pkt, pkt.size);
    if (ret != 0) {
        aic_msg_wait_new_msg(&p_demuxer_data->s_msg, 0);
        p_demuxer_data->need_peek = MM_FALSE;
        return MM_ERROR_EMPTY_DATA;
    }
    p_demuxer_data->get_video_pkt_ok_num++;

    /*Read video data from parser and fill it to decoder packet*/
    p_pkt->data = pkt.data;
    clock_gettime(CLOCK_REALTIME, &before);
    ret = aic_parser_read(p_demuxer_data->p_parser, p_pkt);
    clock_gettime(CLOCK_REALTIME, &after);
    diff = (after.tv_sec - before.tv_sec) * 1000 * 1000 +
           (after.tv_nsec - before.tv_nsec) / 1000;
    if (diff > 42 * 1000) {
        printf("[%s:%d]:%ld\n", __FUNCTION__, __LINE__, diff);
    }

    logi("video aic_parser_read,pts:" FMT_d64 ",type = %d,size:%d,flag:0x%x\n",
         p_pkt->pts, p_pkt->type, p_pkt->size, p_pkt->flag);
    if (!ret) { // read ok
        p_demuxer_data->need_peek = MM_TRUE;
    } else { // now  nothing to do ,becase no other return val
        loge("read video data fail ret %d\n", ret);
        p_demuxer_data->need_peek = MM_FALSE;
        return MM_ERROR_READ_FAILED;
    }

    /*Put packet to decoder*/
    pkt.flag = p_pkt->flag;
    pkt.pts = p_pkt->pts;
    ret = mpp_decoder_put_packet(p_decoder, &pkt);
    if (pkt.flag & PACKET_FLAG_EOS) {
        mm_set_parameter(h_vdec_comp, MM_INDEX_PARAM_VIDEO_STREAM_END_FLAG,
                         NULL);
        logi("strem end flag!!!\n");
    }
    if (ret != 0) {
        p_demuxer_data->put_video_pkt_fail_num++;
    } else {
        p_demuxer_data->put_video_pkt_ok_num++;
    }
    mm_send_command(h_vdec_comp, MM_COMMAND_WKUP, 0, NULL);

    return ret;
}

static int
mm_demuxer_component_process_audio_pkt(mm_demuxer_data *p_demuxer_data,
                                       struct aic_parser_packet *p_pkt)
{
    s32 ret = MM_ERROR_NONE;
    long diff = 0;
    struct mpp_packet pkt = {0};
    struct aic_audio_decoder *p_decoder = NULL;
    struct timespec before = {0}, after = {0};

    /*Get audio decoder handle*/
    mm_component *h_adec_comp =
        p_demuxer_data->out_port_bind[DEMUX_PORT_AUDIO_INDEX].p_bind_comp;
    if (h_adec_comp == NULL) {
        logd("get h_adec_comp is null\n");

        /*if auddec format not support, should peek next video packet*/
        mm_component *h_vdec_comp =
            p_demuxer_data->out_port_bind[DEMUX_PORT_VIDEO_INDEX].p_bind_comp;
        if (h_vdec_comp) {
            p_demuxer_data->need_peek = MM_TRUE;
        }
        return MM_ERROR_NULL_POINTER;
    }
    mm_get_parameter(h_adec_comp, MM_INDEX_PARAM_AUDIO_DECODER_HANDLE,
                     (void *)&p_decoder);
    if (p_decoder == NULL) {
        logd("get audio decoder is null\n");
        return MM_ERROR_NULL_POINTER;
    }

    /*process extra audio pkt and put it to decoder*/
    if (p_demuxer_data->extra_audio_pkt_flag) {
        pkt.size = p_demuxer_data->extra_audio_pkt.size;
        ret = aic_audio_decoder_get_packet(p_decoder, &pkt, pkt.size);
        if (ret != 0) {
            aic_msg_wait_new_msg(&p_demuxer_data->s_msg, 0);
            return MM_ERROR_EMPTY_DATA;
        }

        memcpy(pkt.data, p_demuxer_data->extra_audio_pkt.data, pkt.size);
        p_demuxer_data->get_audio_pkt_ok_num++;
        if (p_demuxer_data->net_stream) {
             pkt.flag = 0;
        } else {
             pkt.flag |= p_demuxer_data->extra_video_pkt.flag;
        }
        pkt.pts = p_demuxer_data->extra_audio_pkt.pts;
        ret = aic_audio_decoder_put_packet(p_decoder, &pkt);
        if (ret != 0) {
            loge("put extra pkt to decoder failed %x!!!\n", ret);
            p_demuxer_data->put_audio_pkt_fail_num++;
            return ret;
        }
        mm_send_command(h_adec_comp, MM_COMMAND_WKUP, 0, NULL);
        logi("send audio extra data,pts:" FMT_d64 ",type = %d,size:%d,flag:0x%x\n",
             p_pkt->pts, p_pkt->type, p_pkt->size, p_pkt->flag);

        p_demuxer_data->put_audio_pkt_ok_num++;
        p_demuxer_data->extra_audio_pkt_flag = MM_FALSE;

        if (p_demuxer_data->extra_audio_pkt.data) {
            mpp_free(p_demuxer_data->extra_audio_pkt.data);
            p_demuxer_data->extra_audio_pkt.data = NULL;
            p_demuxer_data->extra_audio_pkt.size = 0;
        }
    }

    /*Get empty packet from decoder*/
    pkt.size = p_pkt->size;
    ret = aic_audio_decoder_get_packet(p_decoder, &pkt, pkt.size);
    if (ret != 0) {
        aic_msg_wait_new_msg(&p_demuxer_data->s_msg, 0);
        p_demuxer_data->need_peek = MM_FALSE;
        return MM_ERROR_EMPTY_DATA;
    }
    p_demuxer_data->get_audio_pkt_ok_num++;
    /*Read audio data from parser and fill it to decoder packet*/
    p_pkt->data = pkt.data;
    clock_gettime(CLOCK_REALTIME, &before);
    ret = aic_parser_read(p_demuxer_data->p_parser, p_pkt);
    clock_gettime(CLOCK_REALTIME, &after);
    diff = (after.tv_sec - before.tv_sec) * 1000 * 1000 +
           (after.tv_nsec - before.tv_nsec) / 1000;
    if (diff > 42 * 1000) {
        printf("[%s:%d]:%ld\n", __FUNCTION__, __LINE__, diff);
    }

    logi("audio aic_parser_read,pts:" FMT_d64 ",type = %d,size:%d,flag:0x%x\n",
         p_pkt->pts, p_pkt->type, p_pkt->size, p_pkt->flag);

    if (!ret) { // read ok
        p_demuxer_data->need_peek = MM_TRUE;
    } else { // now  nothing to do ,becase no other return val
        loge("read audio data fail\n");
        p_demuxer_data->need_peek = MM_FALSE;
        return MM_ERROR_READ_FAILED;
    }

    /*Put packet to decoder*/
    pkt.flag = p_pkt->flag;
    pkt.pts = p_pkt->pts;
    ret = aic_audio_decoder_put_packet(p_decoder, &pkt);
    if (ret != 0) {
        p_demuxer_data->put_audio_pkt_fail_num++;
    } else {
        p_demuxer_data->put_audio_pkt_ok_num++;
    }
    mm_send_command(h_adec_comp, MM_COMMAND_WKUP, MM_TRUE, NULL);
    return ret;
}

void mm_demuxer_pkt_count_print(mm_demuxer_data *p_demuxer_data)
{
    printf("[%s:%d]video_pkt_num:%u,get_video_pkt_ok_num:%u,"
           "put_video_pkt_ok_num:%u,put_video_pkt_fail_num:%u,"
           "audio_pkt_num:%u,get_audio_pkt_ok_num:%u,"
           "put_audio_pkt_ok_num:%u,put_audio_pkt_fail_num:%u\n",
           __FUNCTION__,__LINE__,
           p_demuxer_data->video_pkt_num,
           p_demuxer_data->get_video_pkt_ok_num,
           p_demuxer_data->put_video_pkt_ok_num,
           p_demuxer_data->put_video_pkt_fail_num,
           p_demuxer_data->audio_pkt_num,
           p_demuxer_data->get_audio_pkt_ok_num,
           p_demuxer_data->put_audio_pkt_ok_num,
           p_demuxer_data->put_audio_pkt_fail_num);
}

static void *mm_demuxer_component_thread(void *p_thread_data)
{
    s32 ret = MM_ERROR_NONE;
    s32 cmd = MM_COMMAND_UNKNOWN;
    MM_BOOL b_notify_frame_end = MM_FALSE;
    struct aic_parser_packet s_pkt;
    mm_demuxer_data *p_demuxer_data = NULL;

    memset(&s_pkt, 0x00, sizeof(struct aic_parser_packet));
    p_demuxer_data = (mm_demuxer_data *)p_thread_data;
    p_demuxer_data->need_peek = MM_TRUE;

    while (1) {
    _AIC_MSG_GET_:

        /* process cmd and change state*/
        cmd = mm_demuxer_component_process_cmd(p_demuxer_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            continue;
        } else if (MM_COMMAND_STOP == cmd) {
            goto _EXIT;
        }

        if (p_demuxer_data->state != MM_STATE_EXECUTING) {
            aic_msg_wait_new_msg(&p_demuxer_data->s_msg, 0);
            continue;
        }

        if (p_demuxer_data->eos) {
            if (!b_notify_frame_end) {
                mm_demuxer_event_notify(p_demuxer_data, MM_EVENT_BUFFER_FLAG, 0,
                                        0, NULL);
                b_notify_frame_end = MM_TRUE;
            }
            aic_msg_wait_new_msg(&p_demuxer_data->s_msg, 0);
            continue;
        }
        b_notify_frame_end = 0;

        /* peek pkt info from parser*/
        if (p_demuxer_data->need_peek) {
            s_pkt.flag = 0;
            s_pkt.size = 0;
            ret = aic_parser_peek(p_demuxer_data->p_parser, &s_pkt);
            if (!ret) {
                logd("peek type %d ok\n", s_pkt.type);
                p_demuxer_data->need_peek = MM_FALSE;
                if (s_pkt.type == MPP_MEDIA_TYPE_VIDEO) {
                    p_demuxer_data->video_pkt_num++;
                } else if (s_pkt.type == MPP_MEDIA_TYPE_AUDIO) {
                    p_demuxer_data->audio_pkt_num++;
                }
            } else if (ret == PARSER_EOS) { //peek end
                printf("[%s:%d]*************PARSER_EOS*******************\n",
                       __FUNCTION__, __LINE__);
                p_demuxer_data->eos = 1;
                if (s_pkt.size == 0 && s_pkt.flag == PACKET_EOS) {
                    mm_demuxer_component_process_eos_pkt(p_demuxer_data);
                }
                goto _AIC_MSG_GET_;
            } else { // now  nothing to do ,becase no other return val
                loge("peek fail\n");
                goto _AIC_MSG_GET_;
            }
        }
        /*skip other pkt type*/
        if (s_pkt.type != MPP_MEDIA_TYPE_VIDEO &&
            s_pkt.type != MPP_MEDIA_TYPE_AUDIO) {
            p_demuxer_data->need_peek = MM_TRUE;
            goto _AIC_MSG_GET_;
        }

        /* read pkt from parser and put it to decoder*/
        if (s_pkt.type == MPP_MEDIA_TYPE_VIDEO) {
            if (p_demuxer_data->skip_track & DEMUX_SKIP_VIDEO_TRACK) {
                p_demuxer_data->need_peek = MM_TRUE;
                goto _AIC_MSG_GET_;
            }
            mm_demuxer_component_process_video_pkt(p_demuxer_data, &s_pkt);
        } else if (s_pkt.type == MPP_MEDIA_TYPE_AUDIO) {
            if (p_demuxer_data->skip_track & DEMUX_SKIP_AUDIO_TRACK) {
                p_demuxer_data->need_peek = MM_TRUE;
                goto _AIC_MSG_GET_;
            }
            mm_demuxer_component_process_audio_pkt(p_demuxer_data, &s_pkt);
        }
    }

_EXIT:
    mm_demuxer_pkt_count_print(p_demuxer_data);
    printf("[%s:%d]mm_demuxer_component_thread exit\n", __FUNCTION__,
           __LINE__);
    return (void *)MM_ERROR_NONE;
}
