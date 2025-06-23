/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: middle media muxer component
 */
#include "mm_muxer_component.h"
#include "aic_message.h"
#include "aic_muxer.h"
#include "mpp_list.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include <malloc.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>
#include <sys/prctl.h>

#define MUX_PACKET_NUM_MAX 64

#define mm_muxer_list_empty(list, mutex) \
    ({                                   \
        int ret = 0;                     \
        pthread_mutex_lock(&mutex);      \
        ret = mpp_list_empty(list);      \
        pthread_mutex_unlock(&mutex);    \
        (ret);                           \
    })

typedef struct mm_muxer_in_packet {
    struct aic_av_packet packet;
    struct mpp_list list;
} mm_muxer_in_packet;

typedef struct mm_muxer_data {
    MM_STATE_TYPE state;
    pthread_mutex_t state_lock;
    mm_callback *p_callback;
    void *p_app_data;
    mm_handle h_self;
    mm_port_param port_param;

    mm_param_port_def in_port_def[2];
    mm_bind_info in_port_bind[2];

    pthread_t thread_id;
    struct aic_message_queue msg;

    struct aic_av_media_info media_info;
    mm_param_content_uri *p_contenturi;

    struct aic_muxer *p_muxer;
    struct mpp_list in_empty_pkt;
    struct mpp_list in_ready_pkt;
    struct mpp_list in_processed_pkt;
    pthread_mutex_t in_pkt_lock;
    u32 in_pkt_node_num;

    u32 max_duration;
    u32 file_num;
    s32 muxer_type;

    u32 receive_frame_num;
    u32 cur_file_write_frame_num;

} mm_muxer_data;

static void *mm_muxer_component_thread(void *p_thread_data);

static s32 mm_muxer_set_command(
    mm_handle h_component,
    MM_COMMAND_TYPE cmd,
    u32 param1,
    void *p_cmd_data)
{
    mm_muxer_data *p_muxer_data;
    s32 error = MM_ERROR_NONE;
    struct aic_message msg;
    p_muxer_data = (mm_muxer_data *)(((mm_component *)h_component)->p_comp_private);
    msg.message_id = cmd;
    msg.param = param1;
    msg.data_size = 0;

    if (p_cmd_data != NULL) {
        msg.data = p_cmd_data;
        msg.data_size = strlen((char *)p_cmd_data);
    }

    aic_msg_put(&p_muxer_data->msg, &msg);
    return error;
}

static s32 mm_muxer_get_parameter(
    mm_handle h_component,
    MM_INDEX_TYPE index,
    void *p_param)
{
    mm_muxer_data *p_muxer_data;
    s32 error = MM_ERROR_NONE;
    p_muxer_data = (mm_muxer_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
    case MM_INDEX_PARAM_PORT_DEFINITION: {
        mm_param_port_def *port = (mm_param_port_def *)p_param;
        if (port->port_index == MUX_PORT_VIDEO_INDEX) {
            memcpy(port, &p_muxer_data->in_port_def[MUX_PORT_VIDEO_INDEX], sizeof(mm_param_port_def));
        } else if (port->port_index == MUX_PORT_AUDIO_INDEX) {
            memcpy(port, &p_muxer_data->in_port_def[MUX_PORT_AUDIO_INDEX], sizeof(mm_param_port_def));
        } else {
            error = MM_ERROR_BAD_PARAMETER;
        }
        break;
    }

    default:
        error = MM_ERROR_UNSUPPORT;
        break;
    }

    return error;
}

static s32 mm_muxer_set_parameter(
    mm_handle h_component,
    MM_INDEX_TYPE index,
    void *p_param)
{
    mm_muxer_data *p_muxer_data;
    s32 error = MM_ERROR_NONE;

    p_muxer_data = (mm_muxer_data *)(((mm_component *)h_component)->p_comp_private);
    switch (index) {
    case MM_INDEX_PARAM_PORT_DEFINITION: {
        // 1 video codec and video info
        mm_param_port_def *p_port = (mm_param_port_def *)p_param;
        if (p_port->port_index == MUX_PORT_VIDEO_INDEX) {
            p_muxer_data->media_info.has_video = 1;
            if (p_port->format.video.compression_format != MM_VIDEO_CODING_MJPEG) {
                loge("now only sup_port MM_VIDEO_CODING_MJPEG");
                break;
            }
            p_muxer_data->media_info.video_stream.codec_type = MPP_CODEC_VIDEO_DECODER_MJPEG;
            p_muxer_data->media_info.video_stream.width = p_port->format.video.frame_width;
            p_muxer_data->media_info.video_stream.height = p_port->format.video.frame_height;
            p_muxer_data->media_info.video_stream.frame_rate = p_port->format.video.framerate;
            p_muxer_data->media_info.video_stream.bit_rate = p_port->format.video.bitrate;
        } else if ((p_port->port_index == MUX_PORT_AUDIO_INDEX)) {
            p_muxer_data->media_info.has_audio = 1;
            p_muxer_data->media_info.audio_stream.codec_type = p_port->format.audio.encoding;
            p_muxer_data->media_info.audio_stream.bit_rate = p_port->format.audio.bitrate;
            p_muxer_data->media_info.audio_stream.nb_channel = p_port->format.audio.channels;
            p_muxer_data->media_info.audio_stream.sample_rate = p_port->format.audio.sample_rate;
            p_muxer_data->media_info.audio_stream.bits_per_sample = 16;
        }
        break;
    }

    case MM_INDEX_PARAM_CONTENT_URI: {
        // file path
        mm_param_content_uri *p_contenturi = (mm_param_content_uri *)p_param;
        p_muxer_data->p_contenturi = (mm_param_content_uri *)mpp_alloc(p_contenturi->size);
        memcpy(p_muxer_data->p_contenturi, p_contenturi, p_contenturi->size);
        break;
    }
    case MM_INDEX_VENDOR_MUXER_RECORD_FILE_INFO: {
        mm_param_record_file_info *p_record_file = (mm_param_record_file_info *)p_param;
        if (p_record_file->muxer_type != AIC_MUXER_TYPE_MP4) {
            loge("not suport muxer type, now only sup_port mp4");
            error = MM_ERROR_BAD_PARAMETER;
            break;
        }
        p_muxer_data->max_duration = p_record_file->duration;
        p_muxer_data->muxer_type = p_record_file->muxer_type;
        p_muxer_data->file_num = p_record_file->file_num;
        break;
    }
    default:
        break;
    }

    return error;
}

static s32 mm_muxer_get_config(
    mm_handle h_component,
    MM_INDEX_TYPE index,
    void *p_config)
{
    s32 error = MM_ERROR_NONE;
    return error;
}

static s32 mm_muxer_set_config(
    mm_handle h_component,
    MM_INDEX_TYPE index,
    void *p_config)
{
    s32 error = MM_ERROR_NONE;
    return error;
}

static s32 mm_muxer_get_state(
    mm_handle h_component,
    MM_STATE_TYPE *p_state)
{
    s32 error = MM_ERROR_NONE;
    mm_muxer_data *p_muxer_data;
    p_muxer_data = (mm_muxer_data *)(((mm_component *)h_component)->p_comp_private);

    pthread_mutex_lock(&p_muxer_data->state_lock);
    *p_state = p_muxer_data->state;
    pthread_mutex_unlock(&p_muxer_data->state_lock);
    return error;
}

static s32 mm_muxer_bind_request(
    mm_handle h_comp,
    u32 port,
    mm_handle h_bind_port,
    u32 bind_port)
{
    s32 error = MM_ERROR_NONE;
    mm_param_port_def *p_port;
    mm_bind_info *p_bind_info;
    mm_muxer_data *p_muxer_data;

    p_muxer_data = (mm_muxer_data *)(((mm_component *)h_comp)->p_comp_private);
    if (p_muxer_data->state != MM_STATE_LOADED) {
        loge("Component is not in MM_STATE_LOADED,it is in%d,it can not tunnel\n",
             p_muxer_data->state);
        return MM_ERROR_INVALID_STATE;
    }
    if (port == MUX_PORT_AUDIO_INDEX) {
        p_port = &p_muxer_data->in_port_def[MUX_PORT_AUDIO_INDEX];
        p_bind_info = &p_muxer_data->in_port_bind[MUX_PORT_AUDIO_INDEX];
    } else if (port == MUX_PORT_VIDEO_INDEX) {
        p_port = &p_muxer_data->in_port_def[MUX_PORT_VIDEO_INDEX];
        p_bind_info = &p_muxer_data->in_port_bind[MUX_PORT_VIDEO_INDEX];
    } else {
        loge("component can not find port :%d\n", port);
        return MM_ERROR_BAD_PARAMETER;
    }

    // cancle setup tunnel
    if (NULL == h_bind_port && 0 == bind_port) {
        p_bind_info->flag = MM_FALSE;
        p_bind_info->bind_port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_port;
        return MM_ERROR_NONE;
    }

    if (p_port->dir == MM_DIR_OUTPUT) {
        p_bind_info->bind_port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_port;
        p_bind_info->flag = MM_TRUE;
    } else if (p_port->dir == MM_DIR_INPUT) {
        mm_param_port_def bind_port_param;
        bind_port_param.port_index = bind_port;
        mm_get_parameter(h_bind_port, MM_INDEX_PARAM_PORT_DEFINITION, &bind_port_param);
        if (bind_port_param.dir != MM_DIR_OUTPUT) {
            loge("both ports are input.\n");
            return MM_ERROR_PORT_NOT_COMPATIBLE;
        }

        p_bind_info->bind_port_index = bind_port;
        p_bind_info->p_bind_comp = h_bind_port;
        p_bind_info->flag = MM_TRUE;
    } else {
        loge("port is neither output nor input.\n");
        return MM_ERROR_PORT_NOT_COMPATIBLE;
    }
    return error;
}

static s32 mm_muxer_send_buffer(
    mm_handle h_component,
    mm_buffer *p_buffer)
{
    s32 error = MM_ERROR_NONE;
    mm_muxer_data *p_muxer_data;
    mm_muxer_in_packet *p_pkt_node;
    struct aic_message msg;
    int port_id;
    p_muxer_data = (mm_muxer_data *)(((mm_component *)h_component)->p_comp_private);

    port_id = p_buffer->output_port_index;
    if (port_id != MUX_PORT_AUDIO_INDEX && port_id != MUX_PORT_VIDEO_INDEX) {
        loge("MM_ERROR_BAD_PARAMETER\n");
        return MM_ERROR_BAD_PARAMETER;
    }

    pthread_mutex_lock(&p_muxer_data->state_lock);
    if (p_muxer_data->state != MM_STATE_EXECUTING) {
        logw("component is not in MM_STATE_EXECUTING,it is in [%d]!!!\n", p_muxer_data->state);
        pthread_mutex_unlock(&p_muxer_data->state_lock);
        return MM_ERROR_SAME_STATE;
    }

    if (mm_muxer_list_empty(&p_muxer_data->in_empty_pkt, p_muxer_data->in_pkt_lock)) {
        mm_muxer_in_packet *p_pkt_node = (mm_muxer_in_packet *)mpp_alloc(sizeof(mm_muxer_in_packet));
        if (NULL == p_pkt_node) {
            loge("MM_ERROR_INSUFFICIENT_RESOURCES\n");
            pthread_mutex_unlock(&p_muxer_data->state_lock);
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }
        memset(p_pkt_node, 0x00, sizeof(mm_muxer_in_packet));
        pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
        mpp_list_add_tail(&p_pkt_node->list, &p_muxer_data->in_empty_pkt);
        pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);
        p_muxer_data->in_pkt_node_num++;
    }

    pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
    p_pkt_node = mpp_list_first_entry(&p_muxer_data->in_empty_pkt, mm_muxer_in_packet, list);
    memcpy(&p_pkt_node->packet, p_buffer->p_buffer, sizeof(struct aic_av_packet));
    mpp_list_del(&p_pkt_node->list);
    mpp_list_add_tail(&p_pkt_node->list, &p_muxer_data->in_ready_pkt);
    p_muxer_data->receive_frame_num++;
    // loge("receive_frame_num:%d\n",p_muxer_data->receive_frame_num);
    pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);

    msg.message_id = MM_COMMAND_NOPS;
    msg.data_size = 0;
    aic_msg_put(&p_muxer_data->msg, &msg);

    pthread_mutex_unlock(&p_muxer_data->state_lock);
    return error;
}

static s32 mm_muxer_giveback_buffer(
    mm_handle h_component,
    mm_buffer *p_buffer)
{
    s32 error = MM_ERROR_NONE;
    return error;
}

static s32 mm_muxer_set_callback(
    mm_handle h_component,
    mm_callback *p_callback,
    void *p_app_data)
{
    s32 error = MM_ERROR_NONE;
    mm_muxer_data *p_muxer_data;
    p_muxer_data = (mm_muxer_data *)(((mm_component *)h_component)->p_comp_private);
    p_muxer_data->p_callback = p_callback;
    p_muxer_data->p_app_data = p_app_data;
    return error;
}

s32 mm_muxer_component_deinit(mm_handle h_component)
{
    s32 error = MM_ERROR_NONE;
    mm_component *p_comp;
    mm_muxer_data *p_muxer_data;
    p_comp = (mm_component *)h_component;
    struct aic_message msg;
    p_muxer_data = (mm_muxer_data *)p_comp->p_comp_private;
    pthread_mutex_lock(&p_muxer_data->state_lock);
    if (p_muxer_data->state != MM_STATE_LOADED) {
        logd("compoent is in %d,but not in MM_STATE_LOADED(1),can ont FreeHandle.\n", p_muxer_data->state);
        pthread_mutex_unlock(&p_muxer_data->state_lock);
        return MM_ERROR_INVALID_STATE;
    }
    pthread_mutex_unlock(&p_muxer_data->state_lock);
    msg.message_id = MM_COMMAND_STOP;
    msg.data_size = 0;
    aic_msg_put(&p_muxer_data->msg, &msg);
    pthread_join(p_muxer_data->thread_id, (void *)&error);

    pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
    if (!mpp_list_empty(&p_muxer_data->in_empty_pkt)) {
        mm_muxer_in_packet *p_pkt_node = NULL, *p_pkt_node1 = NULL;
        mpp_list_for_each_entry_safe(p_pkt_node, p_pkt_node1, &p_muxer_data->in_empty_pkt, list)
        {
            mpp_list_del(&p_pkt_node->list);
            mpp_free(p_pkt_node);
        }
    }
    pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);
    pthread_mutex_destroy(&p_muxer_data->in_pkt_lock);
    pthread_mutex_destroy(&p_muxer_data->state_lock);
    aic_msg_destroy(&p_muxer_data->msg);

    if (p_muxer_data->p_muxer) {
        aic_muxer_destroy(p_muxer_data->p_muxer);
        p_muxer_data->p_muxer = NULL;
    }

    mpp_free(p_muxer_data);
    p_muxer_data = NULL;
    logd("mm_muxer_component_deinit\n");
    return error;
}

s32 mm_muxer_component_init(mm_handle h_component)
{
    mm_component *p_comp;
    mm_muxer_data *p_muxer_data;
    s32 error = MM_ERROR_NONE;
    u32 err;
    u32 i;
    u32 cnt = 0;
    mm_param_port_def *p_audio_port, *p_video_port;

    s8 msg_create = 0;
    s8 init_pkt_lock_init = 0;
    s8 state_lock_init = 0;

    logd("mm_muxer_component_init....");

    p_comp = (mm_component *)h_component;

    p_muxer_data = (mm_muxer_data *)mpp_alloc(sizeof(mm_muxer_data));

    if (NULL == p_muxer_data) {
        loge("mpp_alloc(sizeof(MuxerDATATYPE) fail!\n");
        return MM_ERROR_INSUFFICIENT_RESOURCES;
    }

    memset(p_muxer_data, 0x0, sizeof(mm_muxer_data));
    p_comp->p_comp_private = (void *)p_muxer_data;
    p_muxer_data->state = MM_STATE_LOADED;
    p_muxer_data->h_self = p_comp;

    p_comp->set_callback = mm_muxer_set_callback;
    p_comp->send_command = mm_muxer_set_command;
    p_comp->get_state = mm_muxer_get_state;
    p_comp->get_parameter = mm_muxer_get_parameter;
    p_comp->set_parameter = mm_muxer_set_parameter;
    p_comp->get_config = mm_muxer_get_config;
    p_comp->set_config = mm_muxer_set_config;
    p_comp->bind_request = mm_muxer_bind_request;
    p_comp->deinit = mm_muxer_component_deinit;
    p_comp->send_buffer = mm_muxer_send_buffer;
    p_comp->giveback_buffer = mm_muxer_giveback_buffer;

    p_audio_port = &p_muxer_data->in_port_def[MUX_PORT_AUDIO_INDEX];
    p_video_port = &p_muxer_data->in_port_def[MUX_PORT_VIDEO_INDEX];

    p_audio_port->port_index = MUX_PORT_AUDIO_INDEX;
    p_audio_port->enable = MM_TRUE;
    p_audio_port->dir = MM_DIR_INPUT;

    p_video_port->port_index = MUX_PORT_VIDEO_INDEX;
    p_video_port->enable = MM_TRUE;
    p_video_port->dir = MM_DIR_INPUT;

    mpp_list_init(&p_muxer_data->in_empty_pkt);
    mpp_list_init(&p_muxer_data->in_ready_pkt);
    mpp_list_init(&p_muxer_data->in_processed_pkt);

    cnt = 0;
    for (i = 0; i < MUX_PACKET_NUM_MAX; i++) {
        mm_muxer_in_packet *p_pkt_node = (mm_muxer_in_packet *)mpp_alloc(sizeof(mm_muxer_in_packet));
        if (NULL == p_pkt_node) {
            break;
        }
        memset(p_pkt_node, 0x00, sizeof(mm_muxer_in_packet));
        mpp_list_add_tail(&p_pkt_node->list, &p_muxer_data->in_empty_pkt);
        cnt++;
    }
    if (cnt == 0) {
        loge("mpp_alloc empty video node fail\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }

    if (aic_msg_create(&p_muxer_data->msg) < 0) {
        loge("aic_msg_create fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    msg_create = 1;
    if (pthread_mutex_init(&p_muxer_data->in_pkt_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        goto _EXIT;
    }
    init_pkt_lock_init = 1;
    if (pthread_mutex_init(&p_muxer_data->state_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        goto _EXIT;
    }
    state_lock_init = 1;

    // Create the component thread
    err = pthread_create(&p_muxer_data->thread_id, NULL, mm_muxer_component_thread, p_muxer_data);
    if (err || !p_muxer_data->thread_id) {
        loge("pthread_create fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    return error;

_EXIT:
    if (state_lock_init)
        pthread_mutex_destroy(&p_muxer_data->state_lock);
    if (init_pkt_lock_init)
        pthread_mutex_destroy(&p_muxer_data->in_pkt_lock);
    if (msg_create)
        aic_msg_destroy(&p_muxer_data->msg);
    if (!mpp_list_empty(&p_muxer_data->in_empty_pkt)) {
        mm_muxer_in_packet *p_pkt_node = NULL, *p_pkt_node1 = NULL;
        mpp_list_for_each_entry_safe(p_pkt_node, p_pkt_node1, &p_muxer_data->in_empty_pkt, list)
        {
            mpp_list_del(&p_pkt_node->list);
            mpp_free(p_pkt_node);
        }
    }
    if (p_muxer_data) {
        mpp_free(p_muxer_data);
        p_muxer_data = NULL;
    }
    return error;
}

static void mm_muxer_event_notify(
    mm_muxer_data *p_muxer_data,
    MM_EVENT_TYPE event,
    u32 data1,
    u32 data2,
    void *p_event_data)
{
    if (p_muxer_data && p_muxer_data->p_callback &&
        p_muxer_data->p_callback->event_handler) {
        p_muxer_data->p_callback->event_handler(
            p_muxer_data->h_self,
            p_muxer_data->p_app_data, event,
            data1, data2, p_event_data);
    }
}

static void mm_muxer_state_change_to_invalid(mm_muxer_data *p_muxer_data)
{
    p_muxer_data->state = MM_STATE_INVALID;
    mm_muxer_event_notify(p_muxer_data, MM_EVENT_ERROR,
                          MM_ERROR_INVALID_STATE, 0, NULL);
    mm_muxer_event_notify(p_muxer_data, MM_EVENT_CMD_COMPLETE,
                          MM_COMMAND_STATE_SET, p_muxer_data->state, NULL);
}

static void mm_muxer_state_change_to_idle(mm_muxer_data *p_muxer_data)
{

    if (MM_STATE_LOADED == p_muxer_data->state) {
    } else if (MM_STATE_EXECUTING == p_muxer_data->state) {
        if (p_muxer_data->p_muxer) {
            mm_muxer_in_packet *p_pkt_node = NULL;
            int ret = 0;
            int num = 0;
            while (!mm_muxer_list_empty(&p_muxer_data->in_ready_pkt, p_muxer_data->in_pkt_lock)) {
                pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
                p_pkt_node = mpp_list_first_entry(&p_muxer_data->in_ready_pkt, mm_muxer_in_packet, list);
                pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);
                ret = aic_muxer_write_packet(p_muxer_data->p_muxer, &p_pkt_node->packet);
                if (ret == 0) {
                    pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
                    mpp_list_del(&p_pkt_node->list);
                    mpp_list_add_tail(&p_pkt_node->list, &p_muxer_data->in_processed_pkt);
                    pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);
                    num++;
                    p_muxer_data->cur_file_write_frame_num++;
                } else if (ret == -2) { // AIC_NO_SPACE
                    loge("AIC_NO_SPACE\n");
                } else {
                    loge("ohter error\n");
                }
            }
            loge("num:%d,cur_file_write_frame_num:%d\n", num, p_muxer_data->cur_file_write_frame_num);

            aic_muxer_write_trailer(p_muxer_data->p_muxer);
            aic_muxer_destroy(p_muxer_data->p_muxer);
            p_muxer_data->p_muxer = NULL;
        }

        while (!mm_muxer_list_empty(&p_muxer_data->in_processed_pkt, p_muxer_data->in_pkt_lock)) {
            int ret = 0;
            mm_muxer_in_packet *p_pkt_node = NULL;
            mm_buffer media_buffer;
            mm_bind_info *p_bind_info = NULL;

            pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
            p_pkt_node = mpp_list_first_entry(&p_muxer_data->in_processed_pkt, mm_muxer_in_packet, list);
            pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);
            if (p_pkt_node->packet.type == MPP_MEDIA_TYPE_VIDEO) {
                p_bind_info = &p_muxer_data->in_port_bind[MUX_PORT_VIDEO_INDEX];
                media_buffer.output_port_index = VENC_PORT_OUT_INDEX;
                media_buffer.input_port_index = MUX_PORT_VIDEO_INDEX;
                media_buffer.p_buffer = (u8 *)&p_pkt_node->packet;
            } else if (p_pkt_node->packet.type == MPP_MEDIA_TYPE_AUDIO) {
                p_bind_info = &p_muxer_data->in_port_bind[MUX_PORT_AUDIO_INDEX];
                media_buffer.output_port_index = VENC_PORT_OUT_INDEX;
                media_buffer.input_port_index = MUX_PORT_AUDIO_INDEX;
                media_buffer.p_buffer = (u8 *)&p_pkt_node->packet;
            } else {
                loge("pkt.type error %d", p_pkt_node->packet.type);
            }

            ret = mm_giveback_buffer(p_bind_info->p_bind_comp, &media_buffer);
            if (ret == 0) {
                pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
                mpp_list_del(&p_pkt_node->list);
                mpp_list_add_tail(&p_pkt_node->list, &p_muxer_data->in_empty_pkt);
                pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);
                logd("giveback frame to venc ok");
            } else {
                loge("giveback frame to venc fail");
            }
        }
    } else if (MM_STATE_PAUSE == p_muxer_data->state) {
    } else {
        mm_muxer_event_notify(p_muxer_data, MM_EVENT_ERROR, MM_ERROR_INCORRECT_STATE_TRANSITION, p_muxer_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_muxer_data->state = MM_STATE_IDLE;
    mm_muxer_event_notify(p_muxer_data, MM_EVENT_CMD_COMPLETE, MM_COMMAND_STATE_SET, p_muxer_data->state, NULL);
}

static void mm_muxer_state_change_to_loaded(mm_muxer_data *p_muxer_data)
{
    if (MM_STATE_IDLE == p_muxer_data->state) {
        p_muxer_data->state = MM_STATE_LOADED;
        mm_muxer_event_notify(p_muxer_data, MM_EVENT_CMD_COMPLETE, MM_COMMAND_STATE_SET, p_muxer_data->state, NULL);
    } else {
        mm_muxer_event_notify(p_muxer_data, MM_EVENT_ERROR, MM_ERROR_INCORRECT_STATE_TRANSITION, p_muxer_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
    }
}

static void mm_muxer_state_change_to_executing(mm_muxer_data *p_muxer_data)
{
    if (MM_STATE_IDLE == p_muxer_data->state) {
    } else if (MM_STATE_PAUSE == p_muxer_data->state) {
    } else {
        mm_muxer_event_notify(p_muxer_data, MM_EVENT_ERROR, MM_ERROR_INCORRECT_STATE_TRANSITION, p_muxer_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_muxer_data->state = MM_STATE_EXECUTING;
}

static void mm_muxer_state_change_to_pause(mm_muxer_data *p_muxer_data)
{
    if (MM_STATE_EXECUTING == p_muxer_data->state) {
    } else {
        mm_muxer_event_notify(p_muxer_data, MM_EVENT_ERROR, MM_ERROR_INCORRECT_STATE_TRANSITION, p_muxer_data->state, NULL);
        logd("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_muxer_data->state = MM_STATE_PAUSE;
}

static int mm_muxer_component_process_cmd(mm_muxer_data *p_muxer_data)
{
    s32 cmd = MM_COMMAND_UNKNOWN;
    s32 cmd_data;
    struct aic_message message;

    if (aic_msg_get(&p_muxer_data->msg, &message) == 0) {
        cmd = message.message_id;
        cmd_data = message.param;
        logi("cmd:%d, cmd_data:%d\n", cmd, cmd_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            pthread_mutex_lock(&p_muxer_data->state_lock);
            if (p_muxer_data->state == (MM_STATE_TYPE)(cmd_data)) {
                mm_muxer_event_notify(p_muxer_data, MM_EVENT_ERROR, MM_ERROR_SAME_STATE, 0, NULL);
                pthread_mutex_unlock(&p_muxer_data->state_lock);
                goto CMD_EXIT;
            }
            switch ((MM_STATE_TYPE)(cmd_data)) {
            case MM_STATE_INVALID:
                mm_muxer_state_change_to_invalid(p_muxer_data);
                break;
            case MM_STATE_LOADED: // idel->loaded means stop
                mm_muxer_state_change_to_loaded(p_muxer_data);
                break;
            case MM_STATE_IDLE:
                mm_muxer_state_change_to_idle(p_muxer_data);
                break;
            case MM_STATE_EXECUTING:
                mm_muxer_state_change_to_executing(p_muxer_data);
                break;
            case MM_STATE_PAUSE:
                mm_muxer_state_change_to_pause(p_muxer_data);
                break;
            default:
                break;
            }
            pthread_mutex_unlock(&p_muxer_data->state_lock);
        } else if (MM_COMMAND_STOP == cmd) {
            logi("mm_muxer_component_thread ready to exit!!!\n");
            goto CMD_EXIT;
        }
    }

CMD_EXIT:
    return cmd;
}


static void *mm_muxer_component_thread(void *p_thread_data)
{
    s32 ret = MM_ERROR_NONE;
    s32 cmd = MM_COMMAND_UNKNOWN;
    mm_muxer_data *p_muxer_data = (mm_muxer_data *)p_thread_data;

    mm_muxer_in_packet *p_pkt_node;
    int create_new_file_flag = 1;
    int file_count = 0;
    int video_duration = 0;
    int audio_duration = 0;
    int64_t first_video_pkt_pts = -1;
    int64_t first_audio_pkt_pts = -1;

    while (1) {
    _AIC_MSG_GET_:
        /* process cmd and change state*/
        cmd = mm_muxer_component_process_cmd(p_muxer_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            continue;
        } else if (MM_COMMAND_STOP == cmd) {
            goto _EXIT;
        }

        if (p_muxer_data->state != MM_STATE_EXECUTING) {
            aic_msg_wait_new_msg(&p_muxer_data->msg, 0);
            continue;
        }

        // process data
        if (create_new_file_flag) {
            // close current file
            if (p_muxer_data->p_muxer) {
                aic_muxer_write_trailer(p_muxer_data->p_muxer);
                aic_muxer_destroy(p_muxer_data->p_muxer);
                loge("cur_file_write_frame_num:%d\n", p_muxer_data->cur_file_write_frame_num);
            }
            p_muxer_data->cur_file_write_frame_num = 0;
            // notity need next file
            mm_muxer_event_notify(p_muxer_data, MM_EVENT_MUXER_NEED_NEXT_FILE, 0, 0, NULL);

            // open next file
	    ret = aic_muxer_create(p_muxer_data->p_contenturi->content_uri,
	                         &p_muxer_data->p_muxer, p_muxer_data->muxer_type);
            if (ret != 0 || !p_muxer_data->p_muxer) {
                loge("aic_muxer_create error");
                goto _AIC_MSG_GET_; // EXIT
            }
            if (aic_muxer_init(p_muxer_data->p_muxer, &p_muxer_data->media_info) != 0) {
                loge("aic_muxer_init error");
                goto _AIC_MSG_GET_; // EXIT
            }
            if (aic_muxer_write_header(p_muxer_data->p_muxer) != 0) {
                loge("aic_muxer_write_header error");
                goto _AIC_MSG_GET_; // EXIT
            }
            create_new_file_flag = 0;
            first_video_pkt_pts = -1;
            first_audio_pkt_pts = -1;
        }

        // return pkt to encoder
        while (!mm_muxer_list_empty(&p_muxer_data->in_processed_pkt, p_muxer_data->in_pkt_lock)) {
            mm_muxer_in_packet *p_pkt_node;
            mm_buffer media_buffer;
            mm_bind_info *p_bind_info = NULL;
            ret = 0;
            pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
            p_pkt_node = mpp_list_first_entry(&p_muxer_data->in_processed_pkt, mm_muxer_in_packet, list);
            pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);
            if (p_pkt_node->packet.type == MPP_MEDIA_TYPE_VIDEO) {
                p_bind_info = &p_muxer_data->in_port_bind[MUX_PORT_VIDEO_INDEX];
                media_buffer.output_port_index = VENC_PORT_OUT_INDEX;
                media_buffer.input_port_index = MUX_PORT_VIDEO_INDEX;
                media_buffer.p_buffer = (u8 *)&p_pkt_node->packet;
            } else if (p_pkt_node->packet.type == MPP_MEDIA_TYPE_AUDIO) {
                p_bind_info = &p_muxer_data->in_port_bind[MUX_PORT_AUDIO_INDEX];
                media_buffer.output_port_index = VENC_PORT_OUT_INDEX;
                media_buffer.input_port_index = MUX_PORT_AUDIO_INDEX;
                media_buffer.p_buffer = (u8 *)&p_pkt_node->packet;
            } else {
                loge("pkt.type error %d", p_pkt_node->packet.type);
            }

            ret = mm_giveback_buffer(p_bind_info->p_bind_comp, &media_buffer);
            if (ret == 0) {
                pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
                mpp_list_del(&p_pkt_node->list);
                mpp_list_add_tail(&p_pkt_node->list, &p_muxer_data->in_empty_pkt);
                pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);
                logd("give back frame to venc ok");
            } else {
                // loge("give back frame to venc fail\n");
                break;
            }
        }

        if (mm_muxer_list_empty(&p_muxer_data->in_ready_pkt, p_muxer_data->in_pkt_lock)) {
            aic_msg_wait_new_msg(&p_muxer_data->msg, 0);
            continue;
        }

        pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
        p_pkt_node = mpp_list_first_entry(&p_muxer_data->in_ready_pkt, mm_muxer_in_packet, list);
        pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);

        if (p_pkt_node->packet.type == MPP_MEDIA_TYPE_VIDEO) {
            if (first_video_pkt_pts == -1) {
                first_video_pkt_pts = p_pkt_node->packet.pts;
            }
            video_duration = p_pkt_node->packet.pts - first_video_pkt_pts;
        } else if (p_pkt_node->packet.type == MPP_MEDIA_TYPE_AUDIO) {
            if (first_audio_pkt_pts == -1) {
                first_audio_pkt_pts = p_pkt_node->packet.pts;
            }
            audio_duration = p_pkt_node->packet.pts - first_audio_pkt_pts;
        }

        if (p_muxer_data->media_info.has_video && p_muxer_data->media_info.has_audio) {
            if (video_duration > p_muxer_data->max_duration && audio_duration > p_muxer_data->max_duration) {
                create_new_file_flag = 1;
                file_count++;
            }
        } else if (p_muxer_data->media_info.has_video && !p_muxer_data->media_info.has_audio) {
            if (video_duration > p_muxer_data->max_duration) {
                create_new_file_flag = 1;
                file_count++;
            }
        } else if (!p_muxer_data->media_info.has_video && p_muxer_data->media_info.has_audio) {
            if (audio_duration > p_muxer_data->max_duration) {
                create_new_file_flag = 1;
                file_count++;
            }
        }

        ret = aic_muxer_write_packet(p_muxer_data->p_muxer, &p_pkt_node->packet);
        if (ret == 0) {
            pthread_mutex_lock(&p_muxer_data->in_pkt_lock);
            mpp_list_del(&p_pkt_node->list);
            mpp_list_add_tail(&p_pkt_node->list, &p_muxer_data->in_processed_pkt);
            pthread_mutex_unlock(&p_muxer_data->in_pkt_lock);
            p_muxer_data->cur_file_write_frame_num++;
        } else if (ret == -2) { // AIC_NO_SPACE
            loge("AIC_NO_SPACE\n");
            goto _AIC_MSG_GET_;
        } else {
            loge("ohter error\n");
        }
    }
_EXIT:
    return (void *)MM_ERROR_NONE;
}
