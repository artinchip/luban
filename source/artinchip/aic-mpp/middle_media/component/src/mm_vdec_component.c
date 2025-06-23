/*
 * Copyright (C) 2020-2025 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: middle media vdec component
 */

#include "mm_vdec_component.h"

#include <inttypes.h>

#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mpp_log.h"
#include "mpp_list.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "mpp_decoder.h"
#include "mpp_dec_type.h"

#define VDEC_INPORT_STREAM_END_FLAG      0x01
#define VDEC_OUTPORT_SEND_ALL_FRAME_FLAG 0x08 // consume all frame in readylist

#define VDEC_BITSTREAM_BUFFER_SIZE (1024 * 1024)

typedef struct mm_vdec_data {
    MM_STATE_TYPE state;
    pthread_mutex_t state_lock;
    mm_callback *p_callback;
    void *p_app_data;
    mm_handle h_self;
    mm_port_param port_param;

    mm_param_port_def in_port_def;
    mm_param_port_def out_port_def;

    mm_bind_info in_port_bind;
    mm_bind_info out_port_bind;

    struct mpp_decoder *p_decoder;
    struct decode_config decoder_config;
    enum mpp_codec_type codec_type;

    pthread_t thread_id;
    struct aic_message_queue s_msg;
    s32 flags;
    pthread_mutex_t in_pkt_lock;
    pthread_mutex_t out_frame_lock;
    u32 decoder_ok_num;
    MM_BOOL wait_for_ready_pkt;
    MM_BOOL wait_for_empty_frame;
    MM_BOOL pkt_end_flag;
} mm_vdec_data;

static void *mm_vdec_component_thread(void *p_thread_data);

static s32 mm_vdec_send_command(mm_handle h_component, MM_COMMAND_TYPE cmd,
                                u32 param1, void *p_cmd_data)
{
    mm_vdec_data *p_vdec_data;
    s32 error = MM_ERROR_NONE;
    struct aic_message s_msg;
    p_vdec_data =
        (mm_vdec_data *)(((mm_component *)h_component)->p_comp_private);
    s_msg.message_id = cmd;
    s_msg.param = param1;
    s_msg.data_size = 0;

    //now not use always NULL
    if (p_cmd_data != NULL) {
        s_msg.data = p_cmd_data;
        s_msg.data_size = strlen((char *)p_cmd_data);
    }
    if (MM_COMMAND_WKUP == (s32)cmd) {
        if (p_vdec_data->wait_for_empty_frame == MM_TRUE) {
            pthread_mutex_lock(&p_vdec_data->out_frame_lock);
            aic_msg_put(&p_vdec_data->s_msg, &s_msg);
            p_vdec_data->wait_for_empty_frame = MM_FALSE;
            pthread_mutex_unlock(&p_vdec_data->out_frame_lock);
        } else if (p_vdec_data->wait_for_ready_pkt == MM_TRUE) {
            pthread_mutex_lock(&p_vdec_data->in_pkt_lock);
            aic_msg_put(&p_vdec_data->s_msg, &s_msg);
            p_vdec_data->wait_for_ready_pkt = MM_FALSE;
            pthread_mutex_unlock(&p_vdec_data->in_pkt_lock);
        }
    } else {
        if (MM_COMMAND_EOS == (s32)cmd) {
            p_vdec_data->pkt_end_flag = MM_TRUE;
        }
        aic_msg_put(&p_vdec_data->s_msg, &s_msg);
    }

    return error;
}

static s32 mm_vdec_get_parameter(mm_handle h_component, MM_INDEX_TYPE index,
                                 void *p_param)
{
    mm_vdec_data *p_vdec_data;
    s32 error = MM_ERROR_NONE;

    p_vdec_data =
        (mm_vdec_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
        case MM_INDEX_PARAM_PORT_DEFINITION: {
            mm_param_port_def *port = (mm_param_port_def *)p_param;
            if (port->port_index == VDEC_PORT_IN_INDEX) {
                memcpy(port, &p_vdec_data->in_port_def,
                       sizeof(mm_param_port_def));
            } else if (port->port_index == VDEC_PORT_OUT_INDEX) {
                memcpy(port, &p_vdec_data->out_port_def,
                       sizeof(mm_param_port_def));
            } else {
                error = MM_ERROR_BAD_PARAMETER;
            }
            break;
        }

        case MM_INDEX_PARAM_VIDEO_DECODER_HANDLE:
            *((struct mpp_decoder **)p_param) = p_vdec_data->p_decoder;
            break;
        default:
            error = MM_ERROR_UNSUPPORT;
            break;
    }

    return error;
}

static s32 mm_vdec_video_format_trans(enum mpp_codec_type *p_dest_type,
                                      MM_VIDEO_CODING_TYPE *p_src_type)
{
    s32 ret = MM_ERROR_NONE;
    if (p_dest_type == NULL || p_src_type == NULL) {
        loge("bad params!!!!\n");
        return MM_ERROR_BAD_PARAMETER;
    }
    if (*p_src_type == MM_VIDEO_CODING_AVC) {
        *p_dest_type = MPP_CODEC_VIDEO_DECODER_H264;
    } else if (*p_src_type == MM_VIDEO_CODING_MJPEG) {
        *p_dest_type = MPP_CODEC_VIDEO_DECODER_MJPEG;
    } else {
        loge("unsupport codec %d!!!!\n", *p_src_type);
        ret = MM_ERROR_UNSUPPORT;
    }
    return ret;
}

static s32
mm_vdec_video_pixel_format_trans(enum mpp_pixel_format *p_dest_pix_format,
                                 MM_COLOR_FORMAT_TYPE *p_src_pix_format)
{
    s32 ret = 0;
    if (p_dest_pix_format == NULL || p_src_pix_format == NULL) {
        loge("bad params!!!!\n");
        return MM_ERROR_BAD_PARAMETER;
    }
    if (*p_src_pix_format == MM_COLOR_FORMAT_YUV420P) {
        *p_dest_pix_format = MPP_FMT_YUV420P;
    } else if (*p_src_pix_format == MM_COLOR_FORMAT_NV12) {
        *p_dest_pix_format = MPP_FMT_NV12;
    } else if (*p_src_pix_format == MM_COLOR_FORMAT_NV21) {
        *p_dest_pix_format = MPP_FMT_NV21;
    } else if (*p_src_pix_format == MM_COLOR_FORMAT_RGB565) {
        *p_dest_pix_format = MPP_FMT_RGB_565;
    } else if (*p_src_pix_format == MM_COLOR_FORMAT_ARGB8888) {
        *p_dest_pix_format = MPP_FMT_ARGB_8888;
    } else if (*p_src_pix_format == MM_COLOR_FORMAT_RGB888) {
        *p_dest_pix_format = MPP_FMT_RGB_888;
    } else if (*p_src_pix_format == MM_COLOR_FORMAT_ARGB1555) {
        *p_dest_pix_format = MPP_FMT_ARGB_1555;
    } else {
        *p_dest_pix_format = MPP_FMT_YUV420P;
        loge("unsupport pixformat!!!!\n");
        ret = MM_ERROR_UNSUPPORT;
    }
    return ret;
}

static s32 mm_vdec_set_parameter(mm_handle h_component, MM_INDEX_TYPE index,
                                 void *p_param)
{
    mm_vdec_data *p_vdec_data;
    s32 error = MM_ERROR_NONE;
    enum mpp_codec_type codec_type;
    enum mpp_pixel_format pix_format;
    p_vdec_data =
        (mm_vdec_data *)(((mm_component *)h_component)->p_comp_private);
    switch (index) {
        case MM_INDEX_PARAM_VIDEO_PORT_FORMAT: {
            mm_video_param_port_format *port_format =
                (mm_video_param_port_format *)p_param;
            index = port_format->port_index;
            if (index == VDEC_PORT_IN_INDEX) {
                p_vdec_data->in_port_def.format.video.compression_format =
                    port_format->compression_format;
                p_vdec_data->in_port_def.format.video.color_format =
                    port_format->color_format;
                logw("compression_format:%d,color_format:%d\n",
                     p_vdec_data->in_port_def.format.video.compression_format,
                     p_vdec_data->in_port_def.format.video.color_format);

                if (mm_vdec_video_format_trans(
                        &codec_type, &port_format->compression_format) != 0) {
                    error = MM_ERROR_UNSUPPORT;
                    loge("MM_ERROR_UNSUPPORT\n");
                    break;
                }
                if (mm_vdec_video_pixel_format_trans(
                        &pix_format, &port_format->color_format) != 0) {
                    error = MM_ERROR_UNSUPPORT;
                    loge("MM_ERROR_UNSUPPORT\n");
                    break;
                }
                p_vdec_data->codec_type = codec_type;
                p_vdec_data->decoder_config.pix_fmt = pix_format;
                logw("compression_format:%d,color_format:%d\n",
                     p_vdec_data->codec_type,
                     p_vdec_data->decoder_config.pix_fmt);
                p_vdec_data->decoder_config.bitstream_buffer_size =
                    VDEC_BITSTREAM_BUFFER_SIZE;
                p_vdec_data->decoder_config.extra_frame_num = 1;
                p_vdec_data->decoder_config.packet_count = 10;
            } else if (index == VDEC_PORT_OUT_INDEX) {
                logw("now no need to set out port param\n");
                error = MM_ERROR_UNSUPPORT;
            } else {
                loge("MM_ERROR_BAD_PARAMETER\n");
            }
            break;
        }
        case MM_INDEX_PARAM_PORT_DEFINITION: {
            mm_param_port_def *port = (mm_param_port_def *)p_param;
            index = port->port_index;
            if (index == VDEC_PORT_IN_INDEX) {
                p_vdec_data->in_port_def.format.video.compression_format =
                    port->format.video.compression_format;
                p_vdec_data->in_port_def.format.video.color_format =
                    port->format.video.color_format;
                logw("compression_format:%d,color_format:%d\n",
                     p_vdec_data->in_port_def.format.video.compression_format,
                     p_vdec_data->in_port_def.format.video.color_format);

                if (mm_vdec_video_format_trans(
                        &codec_type, &port->format.video.compression_format) !=
                    0) {
                    error = MM_ERROR_UNSUPPORT;
                    loge("MM_ERROR_UNSUPPORT\n");
                    break;
                }
                if (mm_vdec_video_pixel_format_trans(
                        &pix_format, &port->format.video.color_format) != 0) {
                    error = MM_ERROR_UNSUPPORT;
                    loge("MM_ERROR_UNSUPPORT\n");
                    break;
                }

                /*need to convert */
                p_vdec_data->codec_type = codec_type;
                p_vdec_data->decoder_config.pix_fmt = pix_format;
                loge("compression_format:%d,color_format:%d\n",
                     p_vdec_data->codec_type,
                     p_vdec_data->decoder_config.pix_fmt);
                /*need to define extened mm_index or decide by inner*/
                p_vdec_data->decoder_config.bitstream_buffer_size =
                    VDEC_BITSTREAM_BUFFER_SIZE;
                p_vdec_data->decoder_config.extra_frame_num = 1;
                p_vdec_data->decoder_config.packet_count = 10;
            } else if (index == VDEC_PORT_OUT_INDEX) {
                logw("now no need to set out port param\n");
            } else {
                loge("MM_ERROR_BAD_PARAMETER\n");
                error = MM_ERROR_BAD_PARAMETER;
            }
            break;
        }

        case MM_INDEX_PARAM_VIDEO_STREAM_END_FLAG:
            pthread_mutex_lock(&p_vdec_data->in_pkt_lock);
            p_vdec_data->flags |= VDEC_INPORT_STREAM_END_FLAG;
            pthread_mutex_unlock(&p_vdec_data->in_pkt_lock);
            break;

        default:
            break;
    }

    return error;
}

static s32 mm_vdec_get_state(mm_handle h_component, MM_STATE_TYPE *p_state)
{
    mm_vdec_data *p_vdec_data;
    s32 error = MM_ERROR_NONE;
    p_vdec_data =
        (mm_vdec_data *)(((mm_component *)h_component)->p_comp_private);

    pthread_mutex_lock(&p_vdec_data->state_lock);
    *p_state = p_vdec_data->state;
    pthread_mutex_unlock(&p_vdec_data->state_lock);

    return error;
}

static s32 mm_vdec_get_config(mm_handle h_component, MM_INDEX_TYPE index,
                              void *p_config)
{
    s32 error = MM_ERROR_NONE;

    return error;
}

static s32 mm_vdec_set_config(mm_handle h_component, MM_INDEX_TYPE index,
                              void *p_config)
{
    s32 error = MM_ERROR_NONE;
    MM_STATE_TYPE state = MM_STATE_INVALID;
    MM_STATE_TYPE vendor_state = MM_STATE_INVALID;
    mm_vdec_data *p_vdec_data =
        (mm_vdec_data *)(((mm_component *)h_component)->p_comp_private);
    mm_handle h_vender_comp = p_vdec_data->out_port_bind.p_bind_comp;

    switch (index) {
        case MM_INDEX_CONFIG_TIME_POSITION:
            // 1 wait video Render & Vdec enter pause state
            do {
                mm_vdec_get_state(h_component, &state);
                mm_get_state(h_vender_comp, &vendor_state);
                if (MM_STATE_PAUSE == state && MM_STATE_PAUSE == vendor_state) {
                    break;
                }
            } while (1);

            // 2 clear flag
            p_vdec_data->flags = 0;

            // 3 clear decoder buff
            printf("mpp_decoder_reset\n");
            mpp_decoder_reset(p_vdec_data->p_decoder);

            break;

        default:
            break;
    }
    return error;
}

static s32 mm_vdec_bind_request(mm_handle h_comp, u32 port,
                                mm_handle h_bind_comp, u32 bind_port)
{
    s32 error = MM_ERROR_NONE;
    mm_param_port_def *p_port;
    mm_bind_info *p_bind_info;
    mm_vdec_data *p_vdec_data;
    p_vdec_data = (mm_vdec_data *)(((mm_component *)h_comp)->p_comp_private);
    if (p_vdec_data->state != MM_STATE_LOADED) {
        loge(
            "Component is not in MM_STATE_LOADED,it is in%d,it can not tunnel\n",
            p_vdec_data->state);
        return MM_ERROR_INVALID_STATE;
    }
    if (port == VDEC_PORT_IN_INDEX) {
        p_port = &p_vdec_data->in_port_def;
        p_bind_info = &p_vdec_data->in_port_bind;
    } else if (port == VDEC_PORT_OUT_INDEX) {
        p_port = &p_vdec_data->out_port_def;
        p_bind_info = &p_vdec_data->out_port_bind;
    } else {
        loge("component can not find port :%u\n", port);
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

static s32 mm_vdec_set_callback(mm_handle h_component, mm_callback *p_cb,
                                void *p_app_data)
{
    s32 error = MM_ERROR_NONE;
    mm_vdec_data *p_vdec_data;
    p_vdec_data =
        (mm_vdec_data *)(((mm_component *)h_component)->p_comp_private);
    p_vdec_data->p_callback = p_cb;
    p_vdec_data->p_app_data = p_app_data;
    return error;
}

s32 mm_vdec_component_deinit(mm_handle h_component)
{
    s32 error = MM_ERROR_NONE;
    mm_component *p_comp;
    mm_vdec_data *p_vdec_data;
    struct aic_message s_msg;

    p_comp = (mm_component *)h_component;

    p_vdec_data = (mm_vdec_data *)p_comp->p_comp_private;

    pthread_mutex_lock(&p_vdec_data->state_lock);
    if (p_vdec_data->state != MM_STATE_LOADED) {
        loge(
            "compoent is in %d,but not in MM_STATE_LOADED(1),can not FreeHandle.\n",
            p_vdec_data->state);
        pthread_mutex_unlock(&p_vdec_data->state_lock);
        return MM_ERROR_UNSUPPORT;
    }
    pthread_mutex_unlock(&p_vdec_data->state_lock);

    s_msg.message_id = MM_COMMAND_STOP;
    s_msg.data_size = 0;
    aic_msg_put(&p_vdec_data->s_msg, &s_msg);
    pthread_join(p_vdec_data->thread_id, (void *)&error);

    pthread_mutex_destroy(&p_vdec_data->in_pkt_lock);
    pthread_mutex_destroy(&p_vdec_data->out_frame_lock);
    pthread_mutex_destroy(&p_vdec_data->state_lock);

    aic_msg_destroy(&p_vdec_data->s_msg);

    if (p_vdec_data->p_decoder) {
        mpp_decoder_destory(p_vdec_data->p_decoder);
        p_vdec_data->p_decoder = NULL;
    }

    mpp_free(p_vdec_data);
    p_vdec_data = NULL;

    logd("mm_vdec_component_deinit\n");
    return error;
}



s32 mm_vdec_component_init(mm_handle h_component)
{
    mm_component *p_comp;
    mm_vdec_data *p_vdec_data;
    s32 error = MM_ERROR_NONE;
    u32 err;

    s8 msg_creat = 0;
    s8 pkt_lock_init = 0;
    s8 frame_lock_init = 0;
    s8 state_lock_init = 0;

    logw("mm_vdec_component_init....");

    p_comp = (mm_component *)h_component;

    p_vdec_data = (mm_vdec_data *)mpp_alloc(sizeof(mm_vdec_data));

    if (NULL == p_vdec_data) {
        loge("mpp_alloc(sizeof(mm_vdec_data) fail!");
        return MM_ERROR_INSUFFICIENT_RESOURCES;
    }

    memset(p_vdec_data, 0x0, sizeof(mm_vdec_data));
    p_comp->p_comp_private = (void *)p_vdec_data;
    p_vdec_data->state = MM_STATE_LOADED;
    p_vdec_data->h_self = p_comp;

    p_comp->set_callback = mm_vdec_set_callback;
    p_comp->send_command = mm_vdec_send_command;
    p_comp->get_state = mm_vdec_get_state;
    p_comp->get_parameter = mm_vdec_get_parameter;
    p_comp->set_parameter = mm_vdec_set_parameter;
    p_comp->get_config = mm_vdec_get_config;
    p_comp->set_config = mm_vdec_set_config;
    p_comp->bind_request = mm_vdec_bind_request;
    p_comp->deinit = mm_vdec_component_deinit;

    p_vdec_data->port_param.ports = 2;
    p_vdec_data->port_param.start_port_num = 0x0;

    p_vdec_data->in_port_def.port_index = VDEC_PORT_IN_INDEX;
    p_vdec_data->in_port_def.enable = MM_TRUE;
    p_vdec_data->in_port_def.dir = MM_DIR_INPUT;

    p_vdec_data->out_port_def.port_index = VDEC_PORT_OUT_INDEX;
    p_vdec_data->out_port_def.enable = MM_TRUE;
    p_vdec_data->out_port_def.dir = MM_DIR_OUTPUT;

    p_vdec_data->in_port_bind.port_index = VDEC_PORT_IN_INDEX;
    p_vdec_data->in_port_bind.p_self_comp = h_component;
    p_vdec_data->out_port_bind.port_index = VDEC_PORT_OUT_INDEX;
    p_vdec_data->out_port_bind.p_self_comp = h_component;

    p_vdec_data->pkt_end_flag = MM_FALSE;

    if (pthread_mutex_init(&p_vdec_data->in_pkt_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    pkt_lock_init = 1;

    if (pthread_mutex_init(&p_vdec_data->out_frame_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    frame_lock_init = 1;

    if (aic_msg_create(&p_vdec_data->s_msg) < 0) {
        loge("aic_msg_create fail!");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    msg_creat = 1;

    if (pthread_mutex_init(&p_vdec_data->state_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    state_lock_init = 1;

    // Create the component thread
    err = pthread_create(&p_vdec_data->thread_id, NULL,
                         mm_vdec_component_thread, p_vdec_data);
    if (err) {
        loge("pthread_create fail!");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }

    return error;

_EXIT:
    if (frame_lock_init) {
        pthread_mutex_destroy(&p_vdec_data->out_frame_lock);
    }
    if (pkt_lock_init) {
        pthread_mutex_destroy(&p_vdec_data->in_pkt_lock);
    }
    if (state_lock_init) {
        pthread_mutex_destroy(&p_vdec_data->state_lock);
    }
    if (msg_creat) {
        aic_msg_destroy(&p_vdec_data->s_msg);
    }

    if (p_vdec_data) {
        mpp_free(p_vdec_data);
        p_vdec_data = NULL;
    }

    return error;
}

static void mm_vdec_event_notify(mm_vdec_data *p_vdec_data, MM_EVENT_TYPE event,
                                 u32 data1, u32 data2, void *p_event_data)
{
    if (p_vdec_data && p_vdec_data->p_callback &&
        p_vdec_data->p_callback->event_handler) {
        p_vdec_data->p_callback->event_handler(p_vdec_data->h_self,
                                               p_vdec_data->p_app_data, event,
                                               data1, data2, p_event_data);
    }
}

static void mm_vdec_state_change_to_invalid(mm_vdec_data *p_vdec_data)
{
    p_vdec_data->state = MM_STATE_INVALID;
    mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR, MM_ERROR_INVALID_STATE, 0,
                         NULL);
    mm_vdec_event_notify(p_vdec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_vdec_data->state, NULL);
}

static void mm_vdec_state_change_to_idle(mm_vdec_data *p_vdec_data)
{
    int ret;
    if (p_vdec_data->state == MM_STATE_LOADED) {
        //create decoder
        if (p_vdec_data->p_decoder == NULL) {
            p_vdec_data->p_decoder =
                mpp_decoder_create(p_vdec_data->codec_type);
            if (p_vdec_data->p_decoder == NULL) {
                loge("mpp_decoder_create %d fail!!!!\n",
                     p_vdec_data->codec_type);
                mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_vdec_data->state, NULL);
                return;
            }
            logw("mpp_decoder_create %d ok!\n", p_vdec_data->codec_type);

            ret = mpp_decoder_init(p_vdec_data->p_decoder,
                                   &p_vdec_data->decoder_config);
            if (ret) {
                loge("mpp_decoder_init %d failed", p_vdec_data->codec_type);
                mpp_decoder_destory(p_vdec_data->p_decoder);
                p_vdec_data->p_decoder = NULL;
                mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_vdec_data->state, NULL);
                return;
            }
            logi("mpp_decoder_init ok!\n ");
        }
    } else if (p_vdec_data->state == MM_STATE_PAUSE) {
    } else if (p_vdec_data->state == MM_STATE_EXECUTING) {
    } else {
        mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_vdec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_vdec_data->state = MM_STATE_IDLE;
    mm_vdec_event_notify(p_vdec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_vdec_data->state, NULL);
}

static void mm_vdec_state_change_to_pause(mm_vdec_data *p_vdec_data)
{
    if (p_vdec_data->state == MM_STATE_LOADED) {
    } else if (p_vdec_data->state == MM_STATE_IDLE) {
    } else if (p_vdec_data->state == MM_STATE_EXECUTING) {
    } else {
        mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_vdec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_vdec_data->state = MM_STATE_PAUSE;
    mm_vdec_event_notify(p_vdec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_vdec_data->state, NULL);
}

static void mm_vdec_state_change_to_loaded(mm_vdec_data *p_vdec_data)
{
    if (p_vdec_data->state == MM_STATE_IDLE) {
    } else if (p_vdec_data->state == MM_STATE_EXECUTING) {
    } else if (p_vdec_data->state == MM_STATE_PAUSE) {
    } else {
        mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_vdec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_vdec_data->state = MM_STATE_LOADED;
    mm_vdec_event_notify(p_vdec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_vdec_data->state, NULL);
}

static void mm_vdec_state_change_to_executing(mm_vdec_data *p_vdec_data)
{
    if (p_vdec_data->state == MM_STATE_LOADED) {
        mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_vdec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    } else if (p_vdec_data->state == MM_STATE_IDLE) {
    } else if (p_vdec_data->state == MM_STATE_PAUSE) {
    } else {
        mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_vdec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_vdec_data->state = MM_STATE_EXECUTING;
    mm_vdec_event_notify(p_vdec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_vdec_data->state, NULL);
}

static int mm_vdec_component_process_cmd(mm_vdec_data *p_vdec_data)
{
    s32 cmd = MM_COMMAND_UNKNOWN;
    s32 cmd_data;
    struct aic_message message;

    if (aic_msg_get(&p_vdec_data->s_msg, &message) == 0) {
        cmd = message.message_id;
        cmd_data = message.param;
        logi("cmd:%d, cmd_data:%d\n", cmd, cmd_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            pthread_mutex_lock(&p_vdec_data->state_lock);
            if (p_vdec_data->state == (MM_STATE_TYPE)(cmd_data)) {
                mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR,
                                     MM_ERROR_SAME_STATE, 0, NULL);
                pthread_mutex_unlock(&p_vdec_data->state_lock);
                goto CMD_EXIT;
            }
            switch (cmd_data) {
                case MM_STATE_INVALID:
                    mm_vdec_state_change_to_invalid(p_vdec_data);
                    break;
                case MM_STATE_LOADED:
                    mm_vdec_state_change_to_loaded(p_vdec_data);
                    break;
                case MM_STATE_IDLE:
                    mm_vdec_state_change_to_idle(p_vdec_data);
                    break;
                case MM_STATE_EXECUTING:
                    mm_vdec_state_change_to_executing(p_vdec_data);
                    break;
                case MM_STATE_PAUSE:
                    mm_vdec_state_change_to_pause(p_vdec_data);
                    break;
                default:
                    break;
            }
            pthread_mutex_unlock(&p_vdec_data->state_lock);
        }
        if (MM_COMMAND_STOP == cmd) {
            logw("ready to exit!!!\n");
            goto CMD_EXIT;
        }
    }

CMD_EXIT:
    return cmd;
}

static void *mm_vdec_component_thread(void *p_thread_data)
{
    s32 cmd = MM_COMMAND_UNKNOWN;
    mm_vdec_data *p_vdec_data = (mm_vdec_data *)p_thread_data;
    s32 dec_ret = 0;
    MM_BOOL b_notify_frame_end = 0;
    mm_bind_info *p_bind_demuxer;
    mm_bind_info *p_bind_video_render;
    p_bind_demuxer = &p_vdec_data->in_port_bind;
    p_bind_video_render = &p_vdec_data->out_port_bind;

    while (1) {
    _AIC_MSG_GET_:

        /* process cmd and change state*/
        cmd = mm_vdec_component_process_cmd(p_vdec_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            continue;
        } else if (MM_COMMAND_STOP == cmd) {
            goto _EXIT;
        }

        if (p_vdec_data->state != MM_STATE_EXECUTING) {
            //usleep(1000);
            aic_msg_wait_new_msg(&p_vdec_data->s_msg, 0);
            continue;
        }

        if (p_vdec_data->flags & VDEC_OUTPORT_SEND_ALL_FRAME_FLAG) {
            if (!b_notify_frame_end) {
                //notify app decoder end
                mm_vdec_event_notify(p_vdec_data, MM_EVENT_BUFFER_FLAG, 0, 0,
                                     NULL);

                b_notify_frame_end = 1;
            }
            aic_msg_wait_new_msg(&p_vdec_data->s_msg, 0);
            continue;
        }
        b_notify_frame_end = 0;

        /* do video decode*/
        dec_ret = mpp_decoder_decode(p_vdec_data->p_decoder);
        if (dec_ret == DEC_OK) {
            logd("mpp_decoder_decode ok!!!\n");
            mm_send_command(p_bind_video_render->p_bind_comp,
                            MM_COMMAND_WKUP, 0, NULL);
            p_vdec_data->decoder_ok_num++;
        } else if (dec_ret == DEC_NO_READY_PACKET) {
            pthread_mutex_lock(&p_vdec_data->in_pkt_lock);
            p_vdec_data->wait_for_ready_pkt = MM_TRUE;
            pthread_mutex_unlock(&p_vdec_data->in_pkt_lock);
            mm_send_command(p_bind_demuxer->p_bind_comp,
                            MM_COMMAND_WKUP, 0, NULL);

            if (p_vdec_data->pkt_end_flag) {
                mm_send_command(p_bind_video_render->p_bind_comp,
                            MM_COMMAND_EOS, 0, NULL);
            }
        } else if (dec_ret == DEC_NO_EMPTY_FRAME) {
            pthread_mutex_lock(&p_vdec_data->out_frame_lock);
            p_vdec_data->wait_for_empty_frame = MM_TRUE;
            pthread_mutex_unlock(&p_vdec_data->out_frame_lock);
        } else if (dec_ret == DEC_NO_RENDER_FRAME) {
            loge("mpp_decoder_decode ret:%d !!!\n", dec_ret);
        } else {
            //ASSERT();
            loge("mpp_decoder_decode error serious,do not keep decoding ret:%d"
                 " !!!\n",
                 dec_ret);
            mm_vdec_event_notify(p_vdec_data, MM_EVENT_ERROR,
                                 MM_ERROR_MB_ERRORS_IN_FRAME, 0, NULL);
            p_vdec_data->flags |= VDEC_OUTPORT_SEND_ALL_FRAME_FLAG;
            goto _AIC_MSG_GET_;
        }

        /* wait cmd wkup*/
        if (dec_ret == DEC_NO_READY_PACKET) {
            pthread_mutex_lock(&p_vdec_data->in_pkt_lock);
            if (!(p_vdec_data->flags & VDEC_INPORT_STREAM_END_FLAG)) {
                pthread_mutex_unlock(&p_vdec_data->in_pkt_lock);
                aic_msg_wait_new_msg(&p_vdec_data->s_msg, 0);
            } else {
                pthread_mutex_unlock(&p_vdec_data->in_pkt_lock);
                aic_msg_wait_new_msg(&p_vdec_data->s_msg, 5 * 1000);
            }
        } else if (dec_ret == DEC_NO_EMPTY_FRAME) {
            if (!(p_vdec_data->flags & VDEC_OUTPORT_SEND_ALL_FRAME_FLAG)) {
                aic_msg_wait_new_msg(&p_vdec_data->s_msg, 0);
            }
        }
    }
_EXIT:
    printf("[%s:%d]decoder_ok_num:%u\n", __FUNCTION__, __LINE__,
           p_vdec_data->decoder_ok_num);
    printf("[%s:%d]mm_vdec_component_thread exit\n", __FUNCTION__, __LINE__);
    return (void *)MM_ERROR_NONE;
}
