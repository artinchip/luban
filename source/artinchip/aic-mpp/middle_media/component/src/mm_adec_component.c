/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: middle media adec component
 */

#include "mm_adec_component.h"

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
#include "mpp_decoder.h"
#include "mpp_dec_type.h"
#include "aic_audio_decoder.h"


#define ADEC_INPORT_STREAM_END_FLAG 0x01 //inprot stream end
#define ADEC_OUTPORT_SEND_ALL_FRAME_FLAG 0x08
#define ADEC_BITSTREAM_BUFFER_SIZE (32 * 1024)


typedef struct mm_adec_data {
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

    struct aic_audio_decoder *p_decoder;
    struct aic_audio_decode_config decoder_config;
    enum aic_audio_codec_type code_type;

    pthread_t thread_id;
    pthread_t decode_thread_id;
    struct aic_message_queue s_msg;

    pthread_mutex_t in_pkt_lock;
    pthread_mutex_t out_frame_lock;
    u32 decoder_ok_num;
    MM_BOOL wait_for_ready_pkt;
    MM_BOOL wait_for_empty_frame;

    MM_BOOL flags;
    MM_BOOL pkt_end_flag;
} mm_adec_data;

static void *mm_adec_component_thread(void *p_thread_data);

static s32 mm_adec_send_command(mm_handle h_component, MM_COMMAND_TYPE cmd,
                                u32 param1, void *p_cmd_data)
{
    mm_adec_data *p_adec_data;
    s32 error = MM_ERROR_NONE;
    struct aic_message s_msg;

    p_adec_data =
        (mm_adec_data *)(((mm_component *)h_component)->p_comp_private);
    s_msg.message_id = cmd;
    s_msg.param = param1;
    s_msg.data_size = 0;

    //now not use always NULL
    if (p_cmd_data != NULL) {
        s_msg.data = p_cmd_data;
        s_msg.data_size = strlen((char *)p_cmd_data);
    }

    if (MM_COMMAND_WKUP == (s32)cmd) {
        if (p_adec_data->wait_for_empty_frame == MM_TRUE) {
            pthread_mutex_lock(&p_adec_data->out_frame_lock);
            aic_msg_put(&p_adec_data->s_msg, &s_msg);
            p_adec_data->wait_for_empty_frame = MM_FALSE;
            pthread_mutex_unlock(&p_adec_data->out_frame_lock);
        } else if (p_adec_data->wait_for_ready_pkt == MM_TRUE) {
            pthread_mutex_lock(&p_adec_data->in_pkt_lock);
            aic_msg_put(&p_adec_data->s_msg, &s_msg);
            p_adec_data->wait_for_ready_pkt = MM_FALSE;
            pthread_mutex_unlock(&p_adec_data->in_pkt_lock);
        } else if (param1 == MM_TRUE) {
            aic_msg_put(&p_adec_data->s_msg, &s_msg);
        }
    } else {
        if (MM_COMMAND_EOS == (s32)cmd) {
            p_adec_data->pkt_end_flag = MM_TRUE;
        }
        aic_msg_put(&p_adec_data->s_msg, &s_msg);
    }

    return error;
}

static s32 mm_adec_get_parameter(mm_handle h_component, MM_INDEX_TYPE index,
                                 void *p_param)
{
    mm_adec_data *p_adec_data;
    s32 error = MM_ERROR_NONE;

    p_adec_data =
        (mm_adec_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
        case MM_INDEX_PARAM_PORT_DEFINITION: {
            mm_param_port_def *port = (mm_param_port_def *)p_param;
            if (port->port_index == ADEC_PORT_IN_INDEX) {
                memcpy(port, &p_adec_data->in_port_def,
                       sizeof(mm_param_port_def));
            } else if (port->port_index == ADEC_PORT_OUT_INDEX) {
                memcpy(port, &p_adec_data->out_port_def,
                       sizeof(mm_param_port_def));
            } else {
                error = MM_ERROR_BAD_PARAMETER;
            }
            break;
        }

        case MM_INDEX_PARAM_AUDIO_DECODER_HANDLE:
            *((struct aic_audio_decoder **)p_param) = p_adec_data->p_decoder;
            break;

        default:
            error = MM_ERROR_UNSUPPORT;
            break;
    }

    return error;
}

static s32 mm_adec_audio_format_trans(enum aic_audio_codec_type *p_desType,
                                      MM_AUDIO_CODING_TYPE *p_srcType)
{
    s32 ret = 0;
    if (p_desType == NULL || p_srcType == NULL) {
        loge("bad params!!!!\n");
        return MM_ERROR_BAD_PARAMETER;
    }
    if (*p_srcType == MM_AUDIO_CODING_MP3) {
        *p_desType = MPP_CODEC_AUDIO_DECODER_MP3;
#ifdef AAC_DECODER
    } else if (*p_srcType == MM_AUDIO_CODING_AAC) {
        *p_desType = MPP_CODEC_AUDIO_DECODER_AAC;
#endif
    } else {
        loge("unsupport codec %d!!!!\n", *p_srcType);
        ret = MM_ERROR_UNSUPPORT;
    }
    return ret;
}

static s32 mm_adec_set_parameter(mm_handle h_component, MM_INDEX_TYPE index,
                                 void *p_param)
{
    mm_adec_data *p_adec_data;
    s32 error = MM_ERROR_NONE;

    enum aic_audio_codec_type codec_type;
    p_adec_data =
        (mm_adec_data *)(((mm_component *)h_component)->p_comp_private);

    switch ((s32)index) {
        case MM_INDEX_PARAM_AUDIO_PORT_FORMAT: {
            mm_audio_param_port_format *port_format =
                (mm_audio_param_port_format *)p_param;
            index = port_format->port_index;
            if (index == ADEC_PORT_IN_INDEX) {
                p_adec_data->in_port_def.format.audio.encoding =
                    port_format->encoding;
                logi("encoding:%d\n",
                     p_adec_data->in_port_def.format.audio.encoding);
                if (mm_adec_audio_format_trans(&codec_type,
                                               &port_format->encoding) != 0) {
                    error = MM_ERROR_UNSUPPORT;
                    break;
                }
                p_adec_data->code_type = codec_type;
                logi("code_type:%d\n", p_adec_data->code_type);
                p_adec_data->decoder_config.packet_buffer_size =
                    ADEC_BITSTREAM_BUFFER_SIZE;
                p_adec_data->decoder_config.packet_count = 16;
                p_adec_data->decoder_config.frame_count = 16;
            } else if (index == ADEC_PORT_OUT_INDEX) {
                logw("now no need to set out port param\n");
            } else {
                loge("MM_ERROR_BAD_PARAMETER\n");
            }
            break;
        }
        case MM_INDEX_PARAM_PORT_DEFINITION: {
            mm_param_port_def *port = (mm_param_port_def *)p_param;
            index = port->port_index;
            if (index == ADEC_PORT_IN_INDEX) {
                p_adec_data->in_port_def.format.audio.encoding =
                    port->format.audio.encoding;
                logw("encoding:%d\n",
                     p_adec_data->in_port_def.format.audio.encoding);
                if (mm_adec_audio_format_trans(
                        &codec_type, &port->format.audio.encoding) != 0) {
                    error = MM_ERROR_UNSUPPORT;
                    loge("MM_ERROR_UNSUPPORT\n");
                    break;
                }

                /*need to convert */
                p_adec_data->code_type = codec_type;
                logw("code_type:%d\n", p_adec_data->code_type);
                p_adec_data->decoder_config.packet_buffer_size =
                    ADEC_BITSTREAM_BUFFER_SIZE;
                p_adec_data->decoder_config.packet_count = 8;
                p_adec_data->decoder_config.frame_count = 16;
            } else if (index == ADEC_PORT_OUT_INDEX) {
                logw("now no need to set out port param\n");
            } else {
                loge("MM_ERROR_BAD_PARAMETER\n");
                error = MM_ERROR_BAD_PARAMETER;
            }
        } break;

        default:
            break;
    }
    return error;
}

static s32 mm_adec_get_config(mm_handle h_component, MM_INDEX_TYPE index,
                              void *p_config)
{
    s32 error = MM_ERROR_NONE;

    return error;
}

static s32 mm_adec_set_config(mm_handle h_component, MM_INDEX_TYPE index,
                              void *p_config)
{
    s32 error = MM_ERROR_NONE;

    mm_adec_data *p_adec_data =
        (mm_adec_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
        case MM_INDEX_CONFIG_TIME_POSITION:
            // 1 clear flag
            p_adec_data->flags = 0;
            // 2 clear decoder buff
            aic_audio_decoder_reset(p_adec_data->p_decoder);
            break;

        default:
            break;
    }
    return error;
}

static s32 mm_adec_get_state(mm_handle h_component, MM_STATE_TYPE *p_state)
{
    mm_adec_data *p_adec_data;
    s32 error = MM_ERROR_NONE;
    p_adec_data =
        (mm_adec_data *)(((mm_component *)h_component)->p_comp_private);

    pthread_mutex_lock(&p_adec_data->state_lock);
    *p_state = p_adec_data->state;
    pthread_mutex_unlock(&p_adec_data->state_lock);

    return error;
}

static s32 mm_adec_bind_request(mm_handle h_comp, u32 port,
                                mm_handle h_bind_comp, u32 bind_port)
{
    s32 error = MM_ERROR_NONE;
    mm_param_port_def *p_port;
    mm_bind_info *p_bind_info;
    mm_adec_data *p_adec_data;
    p_adec_data = (mm_adec_data *)(((mm_component *)h_comp)->p_comp_private);
    if (p_adec_data->state != MM_STATE_LOADED) {
        loge(
            "Component is not in MM_STATE_LOADED,it is in%d,it can not tunnel\n",
            p_adec_data->state);
        return MM_ERROR_INVALID_STATE;
    }
    if (port == ADEC_PORT_IN_INDEX) {
        p_port = &p_adec_data->in_port_def;
        p_bind_info = &p_adec_data->in_port_bind;
    } else if (port == ADEC_PORT_OUT_INDEX) {
        p_port = &p_adec_data->out_port_def;
        p_bind_info = &p_adec_data->out_port_bind;
    } else {
        loge("component can not find port :%u\n", port);
        return MM_ERROR_BAD_PARAMETER;
    }

    // cancle setup tunnel
    if (NULL == h_bind_comp && 0 == bind_port) {
        p_bind_info->flag = MM_FALSE;
        p_bind_info->port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_comp;
        return MM_ERROR_NONE;
    }

    if (p_port->dir == MM_DIR_OUTPUT) {
        p_bind_info->port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_comp;
        p_bind_info->flag = MM_TRUE;
    } else if (p_port->dir == MM_DIR_INPUT) {
        mm_param_port_def bind_port_def;
        bind_port_def.port_index = bind_port;
        mm_get_parameter(h_bind_comp, MM_INDEX_PARAM_PORT_DEFINITION,
                         &bind_port_def);
        if (bind_port_def.dir != MM_DIR_OUTPUT) {
            loge("both ports are input.\n");
            return MM_ERROR_PORT_NOT_COMPATIBLE;
        }

        p_bind_info->port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_comp;
        p_bind_info->flag = MM_TRUE;
    } else {
        loge("port is neither output nor input.\n");
        return MM_ERROR_PORT_NOT_COMPATIBLE;
    }
    return error;
}

static s32 mm_adec_set_callback(mm_handle h_component, mm_callback *p_cb,
                                void *p_app_data)
{
    s32 error = MM_ERROR_NONE;
    mm_adec_data *p_adec_data;
    p_adec_data =
        (mm_adec_data *)(((mm_component *)h_component)->p_comp_private);
    p_adec_data->p_callback = p_cb;
    p_adec_data->p_app_data = p_app_data;
    return error;
}

s32 mm_adec_component_deinit(mm_handle h_component)
{
    s32 error = MM_ERROR_NONE;
    mm_component *p_comp;
    mm_adec_data *p_adec_data;

    p_comp = (mm_component *)h_component;
    struct aic_message s_msg;
    p_adec_data = (mm_adec_data *)p_comp->p_comp_private;

    pthread_mutex_lock(&p_adec_data->state_lock);
    if (p_adec_data->state != MM_STATE_LOADED) {
        loge(
            "compoent is in %d,but not in MM_STATE_LOADED(1),can not FreeHandle.\n",
            p_adec_data->state);
        pthread_mutex_unlock(&p_adec_data->state_lock);
        return MM_ERROR_UNSUPPORT;
    }
    pthread_mutex_unlock(&p_adec_data->state_lock);

    s_msg.message_id = MM_COMMAND_STOP;
    s_msg.data_size = 0;
    aic_msg_put(&p_adec_data->s_msg, &s_msg);
    pthread_join(p_adec_data->thread_id, (void *)&error);
    pthread_mutex_destroy(&p_adec_data->in_pkt_lock);
    pthread_mutex_destroy(&p_adec_data->out_frame_lock);
    pthread_mutex_destroy(&p_adec_data->state_lock);

    aic_msg_destroy(&p_adec_data->s_msg);

    if (p_adec_data->p_decoder) {
        aic_audio_decoder_destroy(p_adec_data->p_decoder);
        p_adec_data->p_decoder = NULL;
    }

    mpp_free(p_adec_data);
    p_adec_data = NULL;

    logi("mm_adec_component_deinit\n");
    return error;
}

s32 mm_adec_component_init(mm_handle h_component)
{
    mm_component *p_comp;
    mm_adec_data *p_adec_data;
    s32 error = MM_ERROR_NONE;
    u32 err;

    s8 msg_create = 0;
    s8 in_pkt_lock_init = 0;
    s8 out_frame_lock_init = 0;
    s8 sate_lock_init = 0;

    logw("mm_adec_component_init....\n");

    p_comp = (mm_component *)h_component;

    p_adec_data = (mm_adec_data *)mpp_alloc(sizeof(mm_adec_data));

    if (NULL == p_adec_data) {
        loge("mpp_alloc(sizeof(mm_adec_data) fail!\n");
        return MM_ERROR_INSUFFICIENT_RESOURCES;
    }

    memset(p_adec_data, 0x0, sizeof(mm_adec_data));
    p_comp->p_comp_private = (void *)p_adec_data;
    p_adec_data->state = MM_STATE_LOADED;
    p_adec_data->h_self = p_comp;

    p_comp->set_callback = mm_adec_set_callback;
    p_comp->send_command = mm_adec_send_command;
    p_comp->get_state = mm_adec_get_state;
    p_comp->get_parameter = mm_adec_get_parameter;
    p_comp->set_parameter = mm_adec_set_parameter;
    p_comp->get_config = mm_adec_get_config;
    p_comp->set_config = mm_adec_set_config;
    p_comp->bind_request = mm_adec_bind_request;
    p_comp->deinit = mm_adec_component_deinit;

    p_adec_data->in_port_def.port_index = ADEC_PORT_IN_INDEX;
    p_adec_data->in_port_def.enable = MM_TRUE;
    p_adec_data->in_port_def.dir = MM_DIR_INPUT;

    p_adec_data->out_port_def.port_index = ADEC_PORT_OUT_INDEX;
    p_adec_data->out_port_def.enable = MM_TRUE;
    p_adec_data->out_port_def.dir = MM_DIR_OUTPUT;

    p_adec_data->in_port_bind.port_index = ADEC_PORT_IN_INDEX;
    p_adec_data->in_port_bind.p_self_comp = h_component;
    p_adec_data->out_port_bind.port_index = ADEC_PORT_OUT_INDEX;
    p_adec_data->out_port_bind.p_self_comp = h_component;

    p_adec_data->pkt_end_flag = MM_FALSE;

    if (pthread_mutex_init(&p_adec_data->in_pkt_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    in_pkt_lock_init = 1;

    if (pthread_mutex_init(&p_adec_data->out_frame_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    out_frame_lock_init = 1;

    if (aic_msg_create(&p_adec_data->s_msg) < 0) {
        loge("aic_msg_create fail!");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    msg_create = 1;

    if (pthread_mutex_init(&p_adec_data->state_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    sate_lock_init = 1;

    err = pthread_create(&p_adec_data->thread_id, NULL,
                         mm_adec_component_thread, p_adec_data);
    if (err || !p_adec_data->thread_id) {
        loge("pthread_create fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }

    logi("mm_adec_component_init OK \n");
    return error;

_EXIT:

    if (out_frame_lock_init) {
        pthread_mutex_destroy(&p_adec_data->in_pkt_lock);
    }
    if (in_pkt_lock_init) {
        pthread_mutex_destroy(&p_adec_data->out_frame_lock);
    }
    if (sate_lock_init) {
        pthread_mutex_destroy(&p_adec_data->state_lock);
    }
    if (msg_create) {
        aic_msg_destroy(&p_adec_data->s_msg);
    }

    if (p_adec_data) {
        mpp_free(p_adec_data);
        p_adec_data = NULL;
    }

    return error;
}

static void mm_adec_event_notify(mm_adec_data *p_adec_data, MM_EVENT_TYPE event,
                                 u32 data1, u32 data2, void *p_event_data)
{
    if (p_adec_data && p_adec_data->p_callback &&
        p_adec_data->p_callback->event_handler) {
        p_adec_data->p_callback->event_handler(p_adec_data->h_self,
                                               p_adec_data->p_app_data, event,
                                               data1, data2, p_event_data);
    }
}

static void mm_adec_state_change_to_invalid(mm_adec_data *p_adec_data)
{
    p_adec_data->state = MM_STATE_INVALID;
    mm_adec_event_notify(p_adec_data, MM_EVENT_ERROR, MM_ERROR_INVALID_STATE, 0,
                         NULL);
    mm_adec_event_notify(p_adec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_adec_data->state, NULL);
}

static void mm_adec_state_change_to_loaded(mm_adec_data *p_adec_data)
{
    if (p_adec_data->state == MM_STATE_IDLE) {
        //mm_adec_GiveBackAllFramesToDecoder(p_adec_data);
    } else if (p_adec_data->state == MM_STATE_EXECUTING) {
    } else if (p_adec_data->state == MM_STATE_PAUSE) {
    } else {
        mm_adec_event_notify(p_adec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_adec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_adec_data->state = MM_STATE_LOADED;
    mm_adec_event_notify(p_adec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_adec_data->state, NULL);
}

static void mm_adec_state_change_to_idle(mm_adec_data *p_adec_data)
{
    int ret;
    if (p_adec_data->state == MM_STATE_LOADED) {
        //create decoder
        if (p_adec_data->p_decoder == NULL) {
            p_adec_data->p_decoder =
                aic_audio_decoder_create(p_adec_data->code_type);
            if (p_adec_data->p_decoder == NULL) {
                loge("mpp_decoder_create fail!!!!\n ");
                mm_adec_event_notify(p_adec_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_adec_data->state, NULL);
                loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
                return;
            }
            logi("aic_audio_decoder_create %d ok!\n", p_adec_data->code_type);

            ret = aic_audio_decoder_init(p_adec_data->p_decoder,
                                         &p_adec_data->decoder_config);
            if (ret) {
                loge("mpp_decoder_init %d failed\n", p_adec_data->code_type);
                aic_audio_decoder_destroy(p_adec_data->p_decoder);
                p_adec_data->p_decoder = NULL;
                mm_adec_event_notify(p_adec_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_adec_data->state, NULL);
                loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
                return;
            }
            logi("aic_audio_decoder_init ok!\n ");
        }
    } else if (p_adec_data->state == MM_STATE_PAUSE) {
    } else if (p_adec_data->state == MM_STATE_EXECUTING) {
    } else {
        mm_adec_event_notify(p_adec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_adec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_adec_data->state = MM_STATE_IDLE;
    mm_adec_event_notify(p_adec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_adec_data->state, NULL);
}

static void mm_adec_state_change_to_excuting(mm_adec_data *p_adec_data)
{
    if (p_adec_data->state == MM_STATE_LOADED) {
        mm_adec_event_notify(p_adec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_adec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;

    } else if (p_adec_data->state == MM_STATE_IDLE) {
    } else if (p_adec_data->state == MM_STATE_PAUSE) {
    } else {
        mm_adec_event_notify(p_adec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_adec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_adec_data->state = MM_STATE_EXECUTING;
    mm_adec_event_notify(p_adec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_adec_data->state, NULL);
}

static void mm_adec_state_change_to_pause(mm_adec_data *p_adec_data)
{
    if (p_adec_data->state == MM_STATE_LOADED) {
    } else if (p_adec_data->state == MM_STATE_IDLE) {
    } else if (p_adec_data->state == MM_STATE_EXECUTING) {
    } else {
        mm_adec_event_notify(p_adec_data, MM_EVENT_ERROR,
                             MM_ERROR_INCORRECT_STATE_TRANSITION,
                             p_adec_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_adec_data->state = MM_STATE_PAUSE;
    mm_adec_event_notify(p_adec_data, MM_EVENT_CMD_COMPLETE,
                         MM_COMMAND_STATE_SET, p_adec_data->state, NULL);
}

static int mm_adec_component_process_cmd(mm_adec_data *p_adec_data)
{
    s32 cmd = MM_COMMAND_UNKNOWN;
    s32 cmd_data;
    struct aic_message message;

    if (aic_msg_get(&p_adec_data->s_msg, &message) == 0) {
        cmd = message.message_id;
        cmd_data = message.param;
        logw("cmd:%d, cmd_data:%d\n", cmd, cmd_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            pthread_mutex_lock(&p_adec_data->state_lock);
            if (p_adec_data->state == (MM_STATE_TYPE)(cmd_data)) {
                mm_adec_event_notify(p_adec_data, MM_EVENT_ERROR,
                                     MM_ERROR_SAME_STATE, 0, NULL);
                pthread_mutex_unlock(&p_adec_data->state_lock);
                goto CMD_EXIT;
            }
            switch (cmd_data) {
                case MM_STATE_INVALID:
                    mm_adec_state_change_to_invalid(p_adec_data);
                    break;
                case MM_STATE_LOADED:
                    mm_adec_state_change_to_loaded(p_adec_data);
                    break;
                case MM_STATE_IDLE:
                    mm_adec_state_change_to_idle(p_adec_data);
                    break;
                case MM_STATE_EXECUTING:
                    mm_adec_state_change_to_excuting(p_adec_data);
                    break;
                case MM_STATE_PAUSE:
                    mm_adec_state_change_to_pause(p_adec_data);
                    break;
                default:
                    break;
            }
            pthread_mutex_unlock(&p_adec_data->state_lock);
        } else if (MM_COMMAND_STOP == cmd) {
            logi("mm_adec_component_thread ready to exit!!!\n");
            goto CMD_EXIT;
        }
    }

CMD_EXIT:
    return cmd;
}

static void *mm_adec_component_thread(void *p_thread_data)
{
    s32 cmd = MM_COMMAND_UNKNOWN;
    mm_adec_data *p_adec_data = (mm_adec_data *)p_thread_data;
    s32 dec_ret = 0;
    MM_BOOL b_notify_frame_end = MM_FALSE;
    mm_bind_info *p_bind_demuxer;
    mm_bind_info *p_bind_audio_render;
    p_bind_demuxer = &p_adec_data->in_port_bind;
    p_bind_audio_render = &p_adec_data->out_port_bind;

    while (1) {
        /* process cmd and change state*/
        cmd = mm_adec_component_process_cmd(p_adec_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            logd("cmd %d!!!\n", cmd);
            continue;
        } else if (MM_COMMAND_STOP == cmd) {
            goto _EXIT;
        }

        if (p_adec_data->state != MM_STATE_EXECUTING) {
            logd("state %d!!!\n", p_adec_data->state);
            aic_msg_wait_new_msg(&p_adec_data->s_msg, 0);
            continue;
        }

        if (p_adec_data->flags & ADEC_OUTPORT_SEND_ALL_FRAME_FLAG) {
            logd("flags %d!!!\n", p_adec_data->flags);
            if (!b_notify_frame_end) {
                //notify app decoder end
                mm_adec_event_notify(p_adec_data, MM_EVENT_BUFFER_FLAG,
                                     0, 0, NULL);
                b_notify_frame_end = MM_TRUE;
            }
            aic_msg_wait_new_msg(&p_adec_data->s_msg, 0);
            continue;
        }
        b_notify_frame_end = MM_FALSE;

        /* do audio decode*/
        dec_ret = aic_audio_decoder_decode(p_adec_data->p_decoder);
        if (dec_ret == DEC_OK) {
            logd("aic_audio_decoder_decode ok!!!\n");
            mm_send_command(p_bind_audio_render->p_bind_comp,
                            MM_COMMAND_WKUP, 0, NULL);
            p_adec_data->decoder_ok_num++;
        } else if (dec_ret == DEC_NO_READY_PACKET) {
            mm_send_command(p_bind_demuxer->p_bind_comp,
                            MM_COMMAND_WKUP, 0, NULL);
            pthread_mutex_lock(&p_adec_data->in_pkt_lock);
            p_adec_data->wait_for_ready_pkt = MM_TRUE;
            pthread_mutex_unlock(&p_adec_data->in_pkt_lock);
            if (p_adec_data->pkt_end_flag) {
                mm_send_command(p_bind_audio_render->p_bind_comp,
                                MM_COMMAND_EOS, 0, NULL);
            }
        } else if (dec_ret == DEC_NO_EMPTY_FRAME) {
            pthread_mutex_lock(&p_adec_data->out_frame_lock);
            p_adec_data->wait_for_empty_frame = MM_TRUE;
            pthread_mutex_unlock(&p_adec_data->out_frame_lock);
        } else if (dec_ret == DEC_NO_RENDER_FRAME) {
            logd("aic_audio_decoder_decode error:%d\n", dec_ret);
        } else {
            logd("aic_audio_decoder_decode error:%d\n", dec_ret);
        }

        /* sleep and wait cmd wkup*/
        if (dec_ret == DEC_NO_READY_PACKET) {
            if (!(p_adec_data->flags & ADEC_INPORT_STREAM_END_FLAG)) {
                aic_msg_wait_new_msg(&p_adec_data->s_msg, 0);
            } else {
                aic_msg_wait_new_msg(&p_adec_data->s_msg, 5 * 1000);
            }
        } else if (dec_ret == DEC_NO_EMPTY_FRAME) {
            if (!(p_adec_data->flags & ADEC_OUTPORT_SEND_ALL_FRAME_FLAG)) {
                aic_msg_wait_new_msg(&p_adec_data->s_msg, 0);
            }
        } else {
            aic_msg_wait_new_msg(&p_adec_data->s_msg, 5 * 1000);
        }
    }
_EXIT:
    printf("[%s:%d]decoder_ok_num:%u\n", __FUNCTION__,
           __LINE__, p_adec_data->decoder_ok_num);

    printf("[%s:%d]mm_adec_component_thread exit\n",__FUNCTION__,
           __LINE__);
    return (void *)MM_ERROR_NONE;
}
