/*
 * Copyright (C) 2020-2024 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: middle media audio render component
 */

#include "mm_audio_render_component.h"
#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>

#include "mpp_log.h"
#include "mpp_list.h"
#include "mpp_mem.h"
#include "aic_message.h"
#include "mpp_decoder.h"
#include "aic_audio_render.h"
#include "aic_audio_decoder.h"

#define AUDIO_SAMPLE_PER_FRAME (16 * 1024)
#define AUDIO_RENDER_INPORT_SEND_ALL_FRAME_FLAG  0x02
#define AUDIO_RENDER_WAIT_FRAME_INTERVAL (10 * 1000 * 1000)
#define AUDIO_RENDER_WAIT_FRAME_MAX_TIME (8 * 1000 * 1000)

//#define AUDIO_RENDRE_DUMP_ENABLE
#ifdef AUDIO_RENDRE_DUMP_ENABLE
#define AUDIO_RENDRE_DUMP_FILEPATH "/sdcard/audio.pcm"
#endif


typedef struct mm_audio_render_data {
    MM_STATE_TYPE state;
    pthread_mutex_t state_lock;
    mm_callback *p_callback;
    void *p_app_data;
    mm_handle h_self;

    mm_port_param port_param;
    mm_param_port_def in_port_def[2];
    mm_param_port_def out_port_def[2];

    mm_bind_info in_port_bind[2];
    mm_bind_info out_port_bind[2];

    pthread_t thread_id;
    struct aic_message_queue s_msg;

    struct aic_audio_render *render;
    struct aic_audio_render_attr audio_render_attr;
    struct aic_audio_frame frame;
    s32 volume;
    s32 volume_change;
    s32 layer_id;
    s32 dev_id;

    MM_TIME_CLOCK_STATE clock_state;

    MM_BOOL audio_render_init_flag;
    MM_BOOL frame_end_flag;
    MM_BOOL flags;
    MM_BOOL frame_fisrt_show_flag;
    MM_BOOL wait_ready_frame_flag;

    u32 receive_frame_num;
    u32 show_frame_ok_num;
    u32 show_frame_fail_num;
    u32 giveback_frame_fail_num;
    u32 giveback_frame_ok_num;
    u32 drop_frame_num;
    u32 disp_frame_num;

    s64 pre_frame_pts;
    s64 first_frame_pts;
    s64 pre_correct_media_time;

#ifdef AUDIO_RENDRE_DUMP_ENABLE
    s32 dump_audio_fd;
    s8  *p_dump_audio_file_path;
#endif

} mm_audio_render_data;

static void *mm_audio_render_component_thread(void *p_thread_data);



static s32 mm_audio_render_send_command(mm_handle h_component,
                                        MM_COMMAND_TYPE cmd, u32 param1,
                                        void *p_cmd_data)
{
    s32 error = MM_ERROR_NONE;
    mm_audio_render_data *p_audio_render_data;
    struct aic_message s_msg;

    p_audio_render_data =
        (mm_audio_render_data *)(((mm_component *)h_component)->p_comp_private);
    s_msg.message_id = cmd;
    s_msg.param = param1;
    s_msg.data_size = 0;

    //now not use always NULL
    if (p_cmd_data != NULL) {
        s_msg.data = p_cmd_data;
        s_msg.data_size = strlen((char *)p_cmd_data);
    }
    if (MM_COMMAND_EOS == (s32)cmd) {
        p_audio_render_data->frame_end_flag = MM_TRUE;
    }
    aic_msg_put(&p_audio_render_data->s_msg, &s_msg);
    return error;
}

static s32 mm_audio_render_get_parameter(mm_handle h_component,
                                         MM_INDEX_TYPE index, void *p_param)
{
    mm_audio_render_data *p_audio_render_data;
    s32 error = MM_ERROR_NONE;

    p_audio_render_data =
        (mm_audio_render_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
        case MM_INDEX_VENDOR_AUDIO_RENDER_VOLUME: {
            s32 vol = p_audio_render_data->render->get_volume(
                p_audio_render_data->render);
            ((mm_param_audio_volume *)p_param)->volume = vol;
            break;
        }
        default:
            error = MM_ERROR_UNSUPPORT;
            break;
    }

    return error;
}

static s32 mm_audio_render_set_parameter(mm_handle h_component,
                                         MM_INDEX_TYPE index, void *p_param)
{
    mm_audio_render_data *p_audio_render_data;
    s32 error = MM_ERROR_NONE;

    p_audio_render_data =
        (mm_audio_render_data *)(((mm_component *)h_component)->p_comp_private);
    if (p_param == NULL) {
        loge("param error!!!\n");
        return MM_ERROR_BAD_PARAMETER;
    }
    switch ((s32)index) {
        case MM_INDEX_VENDOR_AUDIO_RENDER_VOLUME: {
            s32 vol = ((mm_param_audio_volume *)p_param)->volume;
            if (vol < 0) {
                p_audio_render_data->volume = 0;
            } else if (vol < 101) {
                p_audio_render_data->volume = vol;
            } else {
                p_audio_render_data->volume = 100;
            }
            p_audio_render_data->volume_change = 1;

            logd("volume:%d,change:%d\n",
                 p_audio_render_data->volume,
                 p_audio_render_data->volume_change);

            break;
        }
        default:
            break;
    }
    return error;
}

static s32 mm_audio_render_get_config(mm_handle h_component,
                                      MM_INDEX_TYPE index, void *p_config)
{
    s32 error = MM_ERROR_NONE;

    return error;
}

static s32 mm_audio_render_set_config(mm_handle h_component,
                                      MM_INDEX_TYPE index, void *p_config)
{
    s32 error = MM_ERROR_NONE;
    mm_audio_render_data *p_audio_render_data =
        (mm_audio_render_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
        case MM_INDEX_CONFIG_TIME_POSITION:
            //1 reset flag
            p_audio_render_data->frame_fisrt_show_flag = MM_TRUE;
            p_audio_render_data->flags = 0;
            p_audio_render_data->clock_state =
                MM_TIME_CLOCK_STATE_WAITING_FOR_START_TIME;
            // 2 reset render
            //aic_audio_render_reset(p_audio_render_data->render);
            aic_audio_render_clear_cache(p_audio_render_data->render);

            break;

        case MM_INDEX_CONFIG_TIME_CLOCK_STATE: {
            mm_time_config_clock_state *p_state =
                (mm_time_config_clock_state *)p_config;
            p_audio_render_data->clock_state = p_state->state;
            printf("[%s:%d]p_audio_render_data->clock_state:%d\n", __FUNCTION__,
                   __LINE__, p_audio_render_data->clock_state);
            break;
        }

        case MM_INDEX_VENDOR_AUDIO_RENDER_INIT: {
            int ret = 0;
            if (!p_audio_render_data->render) {
                ret = aic_audio_render_create(&p_audio_render_data->render);
                if (ret) {
                    error = MM_ERROR_INSUFFICIENT_RESOURCES;
                    logd("aic_audio_render_create error!!!!\n");
                    break;
                }
                ret = p_audio_render_data->render->init(p_audio_render_data->render,
                                                        p_audio_render_data->dev_id);
                if (!ret) {
                    logd("p_audio_render_data->render->init ok\n");
                    p_audio_render_data->audio_render_init_flag = 1;
                } else {
                    loge("p_audio_render_data->render->init fail\n");
                    error = MM_ERROR_INSUFFICIENT_RESOURCES;
                }
            }
            break;
        }

        default:
            break;
    }

    return error;
}

static s32 mm_audio_render_get_state(mm_handle h_component,
                                     MM_STATE_TYPE *p_state)
{
    mm_audio_render_data *p_audio_render_data;
    s32 error = MM_ERROR_NONE;
    p_audio_render_data =
        (mm_audio_render_data *)(((mm_component *)h_component)->p_comp_private);

    pthread_mutex_lock(&p_audio_render_data->state_lock);
    *p_state = p_audio_render_data->state;
    pthread_mutex_unlock(&p_audio_render_data->state_lock);

    return error;
}

static s32 mm_audio_render_bind_request(mm_handle h_comp, u32 port,
                                        mm_handle h_bind_comp, u32 bind_port)
{
    s32 error = MM_ERROR_NONE;
    mm_param_port_def *p_port;
    mm_bind_info *p_bind_info;

    mm_audio_render_data *p_audio_render_data;
    p_audio_render_data =
        (mm_audio_render_data *)(((mm_component *)h_comp)->p_comp_private);
    if (p_audio_render_data->state != MM_STATE_LOADED) {
        loge(
            "Component is not in MM_STATE_LOADED,it is in%d,it can not tunnel\n",
            p_audio_render_data->state);
        return MM_ERROR_INVALID_STATE;
    }

    if (port == AUDIO_RENDER_PORT_IN_AUDIO_INDEX) {
        p_port =
            &p_audio_render_data->in_port_def[AUDIO_RENDER_PORT_IN_AUDIO_INDEX];
        p_bind_info =
            &p_audio_render_data->in_port_bind[AUDIO_RENDER_PORT_IN_AUDIO_INDEX];

    } else if (port == AUDIO_RENDER_PORT_IN_CLOCK_INDEX) {
        p_port =
            &p_audio_render_data->in_port_def[AUDIO_RENDER_PORT_IN_CLOCK_INDEX];
        p_bind_info =
            &p_audio_render_data->in_port_bind[AUDIO_RENDER_PORT_IN_CLOCK_INDEX];
    } else {
        loge("component can not find port:%u\n", port);
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

        mm_get_parameter(h_bind_comp, MM_INDEX_CONFIG_TIME_POSITION,
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

static s32 mm_audio_render_set_callback(mm_handle h_component,
                                        mm_callback *p_cb, void *p_app_data)
{
    s32 error = MM_ERROR_NONE;
    mm_audio_render_data *p_audio_render_data;
    p_audio_render_data =
        (mm_audio_render_data *)(((mm_component *)h_component)->p_comp_private);
    p_audio_render_data->p_callback = p_cb;
    p_audio_render_data->p_app_data = p_app_data;
    return error;
}

s32 mm_audio_render_component_deinit(mm_handle h_component)
{
    s32 error = MM_ERROR_NONE;
    mm_component *p_comp;
    struct aic_message s_msg;
    mm_audio_render_data *p_audio_render_data;

    p_comp = (mm_component *)h_component;

    p_audio_render_data = (mm_audio_render_data *)p_comp->p_comp_private;

    pthread_mutex_lock(&p_audio_render_data->state_lock);
    if (p_audio_render_data->state != MM_STATE_LOADED) {
        loge(
            "compoent is in %d,but not in MM_STATE_LOADED(1),can ont FreeHandle.\n",
            p_audio_render_data->state);
        pthread_mutex_unlock(&p_audio_render_data->state_lock);
        return MM_ERROR_UNSUPPORT;
    }
    pthread_mutex_unlock(&p_audio_render_data->state_lock);

    s_msg.message_id = MM_COMMAND_STOP;
    s_msg.data_size = 0;
    aic_msg_put(&p_audio_render_data->s_msg, &s_msg);
    pthread_join(p_audio_render_data->thread_id, (void *)&error);

    pthread_mutex_destroy(&p_audio_render_data->state_lock);

    aic_msg_destroy(&p_audio_render_data->s_msg);

    if (p_audio_render_data->render) {
        aic_audio_render_destroy(p_audio_render_data->render);
        p_audio_render_data->render = NULL;
    }

    mpp_free(p_audio_render_data);
    p_audio_render_data = NULL;

    logi("mm_audio_render_component_deinit\n");
    return error;
}

s32 mm_audio_render_component_init(mm_handle h_component)
{
    mm_component *p_comp;
    mm_audio_render_data *p_audio_render_data;
    s32 error = MM_ERROR_NONE;
    u32 err = MM_ERROR_NONE;
    s32 port_index = 0;
    s8 msg_create = 0;
    s8 state_lock_init = 0;

    logi("mm_audio_render_component_init....\n");

    p_comp = (mm_component *)h_component;

    p_audio_render_data =
        (mm_audio_render_data *)mpp_alloc(sizeof(mm_audio_render_data));

    if (NULL == p_audio_render_data) {
        loge("mpp_alloc(sizeof(mm_audio_render_data) fail!");
        return MM_ERROR_INSUFFICIENT_RESOURCES;
    }

    memset(p_audio_render_data, 0x0, sizeof(mm_audio_render_data));
    p_comp->p_comp_private = (void *)p_audio_render_data;
    p_audio_render_data->frame_fisrt_show_flag = MM_TRUE;
    p_audio_render_data->state = MM_STATE_LOADED;
    p_audio_render_data->h_self = p_comp;

    p_comp->set_callback = mm_audio_render_set_callback;
    p_comp->send_command = mm_audio_render_send_command;
    p_comp->get_state = mm_audio_render_get_state;
    p_comp->get_parameter = mm_audio_render_get_parameter;
    p_comp->set_parameter = mm_audio_render_set_parameter;
    p_comp->get_config = mm_audio_render_get_config;
    p_comp->set_config = mm_audio_render_set_config;
    p_comp->bind_request = mm_audio_render_bind_request;
    p_comp->deinit = mm_audio_render_component_deinit;

    port_index = AUDIO_RENDER_PORT_IN_AUDIO_INDEX;
    p_audio_render_data->in_port_def[port_index].port_index = port_index;
    p_audio_render_data->in_port_def[port_index].enable = MM_TRUE;
    p_audio_render_data->in_port_def[port_index].dir = MM_DIR_INPUT;
    port_index = AUDIO_RENDER_PORT_IN_CLOCK_INDEX;
    p_audio_render_data->in_port_def[port_index].port_index = port_index;
    p_audio_render_data->in_port_def[port_index].enable = MM_TRUE;
    p_audio_render_data->in_port_def[port_index].dir = MM_DIR_INPUT;

    p_audio_render_data->frame_end_flag = MM_FALSE;

    if (aic_msg_create(&p_audio_render_data->s_msg) < 0) {
        loge("aic_msg_create fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    msg_create = 1;

    p_audio_render_data->clock_state = MM_TIME_CLOCK_STATE_STOPPED;

    if (pthread_mutex_init(&p_audio_render_data->state_lock, NULL)) {
        loge("pthread_mutex_init fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }
    state_lock_init = 1;

    // Create the component thread
    err = pthread_create(&p_audio_render_data->thread_id, NULL,
                         mm_audio_render_component_thread, p_audio_render_data);
    if (err) {
        loge("pthread_create fail!\n");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT;
    }

    logi("mm_audio_render_component_init OK\n");

#ifdef AUDIO_RENDRE_DUMP_ENABLE
    p_audio_render_data->p_dump_audio_file_path = (s8 *)AUDIO_RENDRE_DUMP_FILEPATH;
    p_audio_render_data->dump_audio_fd = 0;
#endif

    return error;

_EXIT:
    if (state_lock_init) {
        pthread_mutex_destroy(&p_audio_render_data->state_lock);
    }

    if (msg_create) {
        aic_msg_destroy(&p_audio_render_data->s_msg);
    }

    if (p_audio_render_data) {
        mpp_free(p_audio_render_data);
        p_audio_render_data = NULL;
    }
    return error;
}

static void
mm_audio_render_event_notify(mm_audio_render_data *p_audio_render_data,
                             MM_EVENT_TYPE event, u32 data1, u32 data2,
                             void *p_event_data)
{
    if (p_audio_render_data && p_audio_render_data->p_callback &&
        p_audio_render_data->p_callback->event_handler) {
        p_audio_render_data->p_callback->event_handler(
            p_audio_render_data->h_self, p_audio_render_data->p_app_data, event,
            data1, data2, p_event_data);
    }
}

static void mm_audio_render_state_change_to_invalid(
    mm_audio_render_data *p_audio_render_data)
{
    p_audio_render_data->state = MM_STATE_INVALID;
    mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_ERROR,
                                 MM_ERROR_INVALID_STATE, 0, NULL);
    mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_audio_render_data->state, NULL);
}
static void mm_audio_render_state_change_to_loaded(
    mm_audio_render_data *p_audio_render_data)
{
    if (p_audio_render_data->state == MM_STATE_IDLE) {
    } else if (p_audio_render_data->state == MM_STATE_EXECUTING) {
    } else if (p_audio_render_data->state == MM_STATE_PAUSE) {
    } else {
        mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_audio_render_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_audio_render_data->state = MM_STATE_LOADED;
    mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_audio_render_data->state, NULL);
}

static void
mm_audio_render_state_change_to_idle(mm_audio_render_data *p_audio_render_data)
{
    int ret = 0;
    if (p_audio_render_data->state == MM_STATE_LOADED) {
        //create Audio_handle
        if (!p_audio_render_data->render) {
            ret = aic_audio_render_create(&p_audio_render_data->render);
        }

        if (ret != 0) {
            loge("aic_create_Audio_render fail\n");
            mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_ERROR,
                                         MM_ERROR_INCORRECT_STATE_TRANSITION,
                                         p_audio_render_data->state, NULL);

            loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
            return;
        }
    } else if (p_audio_render_data->state == MM_STATE_PAUSE) {
        //aic_audio_render_pause(p_audio_render_data->render);
    } else if (p_audio_render_data->state == MM_STATE_EXECUTING) {
    } else {
        mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_audio_render_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_audio_render_data->state = MM_STATE_IDLE;
    mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_audio_render_data->state, NULL);
}

static void mm_audio_render_state_change_to_excuting(
    mm_audio_render_data *p_audio_render_data)
{
    if (p_audio_render_data->state == MM_STATE_LOADED) {
        mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_audio_render_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    } else if (p_audio_render_data->state == MM_STATE_IDLE) {
    } else if (p_audio_render_data->state == MM_STATE_PAUSE) {
        aic_audio_render_pause(p_audio_render_data->render);

    } else {
        mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_audio_render_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_audio_render_data->state = MM_STATE_EXECUTING;
    mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_audio_render_data->state, NULL);
}

static void
mm_audio_render_state_change_to_pause(mm_audio_render_data *p_audio_render_data)
{
    if (p_audio_render_data->state == MM_STATE_LOADED) {
    } else if (p_audio_render_data->state == MM_STATE_IDLE) {
    } else if (p_audio_render_data->state == MM_STATE_EXECUTING) {
        aic_audio_render_pause(p_audio_render_data->render);
    } else {
        mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_ERROR,
                                     MM_ERROR_INCORRECT_STATE_TRANSITION,
                                     p_audio_render_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_audio_render_data->state = MM_STATE_PAUSE;
    mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_CMD_COMPLETE,
                                 MM_COMMAND_STATE_SET,
                                 p_audio_render_data->state, NULL);
}

#define CORRECT_REF_CLOCK_TIME (1 * 1000 * 1000)

static int mm_process_audio_sync(mm_audio_render_data *p_audio_render_data,
                                 struct aic_audio_frame *p_frame_info)
{
    s64 diff_time;
    s64 real_audio_time;
    s64 audio_cache_duration;
    mm_time_config_timestamp timestamp;
    s32 data1, data2;

    mm_bind_info *p_bind_clock =
        &p_audio_render_data->in_port_bind[AUDIO_RENDER_PORT_IN_CLOCK_INDEX];
    audio_cache_duration = p_audio_render_data->render->get_cached_time(
        p_audio_render_data->render);
    real_audio_time = p_frame_info->pts - audio_cache_duration;
    if (p_bind_clock->flag) {
        mm_get_config(p_bind_clock->p_bind_comp,
                      MM_INDEX_CONFIG_TIME_CUR_MEDIA_TIME, &timestamp);
        diff_time =
            timestamp.timestamp - p_audio_render_data->pre_correct_media_time;
        // correct ref clock per 10s
        if (diff_time > CORRECT_REF_CLOCK_TIME
            || MPP_ABS(real_audio_time,timestamp.timestamp) > CORRECT_REF_CLOCK_TIME) { //correct ref time
            timestamp.timestamp = real_audio_time;
            p_audio_render_data->pre_correct_media_time = timestamp.timestamp;
            mm_set_config(p_bind_clock->p_bind_comp,
                          MM_INDEX_CONFIG_TIME_CUR_AUDIO_REFERENCE, &timestamp);
        }
    }
    data1 = (real_audio_time >> 32) & 0x00000000ffffffff;
    data2 = real_audio_time & 0x00000000ffffffff;
    mm_audio_render_event_notify(p_audio_render_data, MM_EVENT_AUDIO_RENDER_PTS,
                                 data1, data2, NULL);
    return 0;
}

#ifdef AUDIO_RENDRE_DUMP_ENABLE
static int mm_audio_render_dump(mm_audio_render_data *p_audio_render_data,
                                char *data, int len)
{
    int end = 0;
    if (p_audio_render_data->frame.flag & FRAME_FLAG_EOS) {
        end = 1;
        printf("[%s:%d]audio stream end \n", __FUNCTION__, __LINE__);
    }

    if (p_audio_render_data->dump_audio_fd == 0) {
        s32 fd;
        fd = open((char *)p_audio_render_data->p_dump_audio_file_path,
                  O_RDWR | O_CREAT);
        if (fd < 0) {
            loge("open %s failed!!!!!\n",
                 p_audio_render_data->p_dump_audio_file_path);
            return -1;
        } else {
            p_audio_render_data->dump_audio_fd = fd;
        }
    }
    write(p_audio_render_data->dump_audio_fd, data, len);
    if (end == 1) {
        close(p_audio_render_data->dump_audio_fd);
        p_audio_render_data->dump_audio_fd = 0;
    }

    return 0;
}
#endif

static s32 mm_audio_render_get_frame(mm_handle h_adec_comp,
                                     struct aic_audio_frame *p_frame)
{
    s32 ret = DEC_OK;
    struct aic_audio_decoder *p_decoder = NULL;
    mm_get_parameter(h_adec_comp, MM_INDEX_PARAM_AUDIO_DECODER_HANDLE,
                     (void *)&p_decoder);
    if (p_decoder == NULL) {
        return DEC_ERR_NULL_PTR;
    }

    ret = aic_audio_decoder_get_frame(p_decoder, p_frame);
    if (ret != DEC_OK) {
        return ret;
    }
    return ret;
}

static void mm_audio_render_wait_frame_timeout(mm_audio_render_data *p_audio_render_data)
{
    struct timespec before = { 0 }, after = { 0 };
    clock_gettime(CLOCK_REALTIME, &before);
    if (p_audio_render_data->frame_end_flag) {
        printf("[%s:%d]:receive audio frame end flag\n", __FUNCTION__, __LINE__);
        p_audio_render_data->flags |= AUDIO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
        return;
    }
    /*if no empty frame then goto sleep, wait wkup by vdec*/
    aic_msg_wait_new_msg(&p_audio_render_data->s_msg,
                         AUDIO_RENDER_WAIT_FRAME_INTERVAL);
    clock_gettime(CLOCK_REALTIME, &after);
    long diff = (after.tv_sec - before.tv_sec) * 1000 * 1000 +
                (after.tv_nsec - before.tv_nsec) / 1000;

    /*if the get frame diff time overange max wait time, then indicate fame end*/
    if (diff > AUDIO_RENDER_WAIT_FRAME_MAX_TIME) {
        printf("[%s:%d]:%ld\n", __FUNCTION__, __LINE__, diff);
        p_audio_render_data->flags |= AUDIO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
    }
}


static s32 mm_audio_render_put_frame(mm_handle h_adec_comp,
                                     struct aic_audio_frame *p_frame)
{
    s32 ret = DEC_OK;
    struct aic_audio_decoder *p_decoder = NULL;
    mm_get_parameter(h_adec_comp, MM_INDEX_PARAM_AUDIO_DECODER_HANDLE,
                     (void *)&p_decoder);
    if (p_decoder == NULL) {
        return DEC_ERR_NULL_PTR;
    }

    ret = aic_audio_decoder_put_frame(p_decoder, p_frame);
    if (ret != DEC_OK) {
        return ret;
    }
    return ret;
}

static int
mm_audio_render_component_process_cmd(mm_audio_render_data *p_audio_render_data)
{
    s32 cmd = MM_COMMAND_UNKNOWN;
    s32 cmd_data;
    struct aic_message message;

    if (aic_msg_get(&p_audio_render_data->s_msg, &message) == 0) {
        cmd = message.message_id;
        cmd_data = message.param;
        logi("cmd:%d, cmd_data:%d\n", cmd, cmd_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            pthread_mutex_lock(&p_audio_render_data->state_lock);
            if (p_audio_render_data->state == (MM_STATE_TYPE)(cmd_data)) {
                logi("MM_ERROR_SAME_STATE\n");
                mm_audio_render_event_notify(p_audio_render_data,
                                             MM_EVENT_ERROR,
                                             MM_ERROR_SAME_STATE, 0, NULL);
                pthread_mutex_unlock(&p_audio_render_data->state_lock);
                goto CMD_EXIT;
            }
            switch (cmd_data) {
                case MM_STATE_INVALID:
                    mm_audio_render_state_change_to_invalid(
                        p_audio_render_data);
                    break;
                case MM_STATE_LOADED:
                    mm_audio_render_state_change_to_loaded(p_audio_render_data);
                    break;
                case MM_STATE_IDLE:
                    mm_audio_render_state_change_to_idle(p_audio_render_data);
                    break;
                case MM_STATE_EXECUTING:
                    mm_audio_render_state_change_to_excuting(
                        p_audio_render_data);
                    break;
                case MM_STATE_PAUSE:
                    mm_audio_render_state_change_to_pause(p_audio_render_data);
                    break;
                default:
                    break;
            }
            pthread_mutex_unlock(&p_audio_render_data->state_lock);
        } else if (MM_COMMAND_STOP == cmd) {
            logi("mm_audio_render_component_thread ready to exit!!!\n");
            goto CMD_EXIT;
        }
    }

CMD_EXIT:
    return cmd;
}


static void mm_audio_render_calc_frame_num(mm_audio_render_data *p_audio_render_data)
{
    static int render_frame_num = 0;
    static struct timespec pre = { 0 }, cur = { 0 };
    long diff;

    render_frame_num++;
    clock_gettime(CLOCK_REALTIME, &cur);
    diff = (cur.tv_sec - pre.tv_sec) * 1000 * 1000 +
           (cur.tv_nsec - pre.tv_nsec) / 1000;
    if (diff > 1 * 1000 * 1000) {
        pre = cur;
        render_frame_num = 0;
    }
}

static void mm_audio_render_set_attr(mm_audio_render_data *p_audio_render_data)
{
    struct aic_audio_render_attr ao_attr;
    if (!p_audio_render_data->audio_render_init_flag) {
        p_audio_render_data->render->init(p_audio_render_data->render,
                                            p_audio_render_data->dev_id);
        p_audio_render_data->audio_render_init_flag = 1;
    }

    ao_attr.bits_per_sample =
        p_audio_render_data->frame.bits_per_sample;
    ao_attr.channels = p_audio_render_data->frame.channels;
    ao_attr.sample_rate = p_audio_render_data->frame.sample_rate;

    /*need to define a member of struct*/
    ao_attr.smples_per_frame = AUDIO_SAMPLE_PER_FRAME;
    p_audio_render_data->audio_render_attr = ao_attr;
    p_audio_render_data->render->set_attr(p_audio_render_data->render,
                                            &ao_attr);
    if (p_audio_render_data->volume_change) {
        if (p_audio_render_data->render->set_volume(
                p_audio_render_data->render,
                p_audio_render_data->volume) == 0) {
            p_audio_render_data->volume_change = 0;
        } else {
            loge("set_volume error\n");
        }
    } else {
        p_audio_render_data->volume =
            p_audio_render_data->render->get_volume(
                p_audio_render_data->render);
        logd("volume :%d\n", p_audio_render_data->volume);
    }

    printf("[%s:%d]bits_per_sample:%d,channels:%d,sample_rate:%d,pts:"FMT_d64"\n",
           __FUNCTION__, __LINE__, p_audio_render_data->frame.bits_per_sample,
           p_audio_render_data->frame.channels,
           p_audio_render_data->frame.sample_rate,
           p_audio_render_data->frame.pts);
}


void mm_audio_render_frame_count_print(mm_audio_render_data *p_audio_render_data)
{
    printf("[%s:%d]receive_frame_num:%u,"
           "show_frame_ok_num:%u,"
           "show_frame_fail_num:%u,"
           "giveback_frame_ok_num:%u,"
           "giveback_frame_fail_num:%u\n",
           __FUNCTION__, __LINE__, p_audio_render_data->receive_frame_num,
           p_audio_render_data->show_frame_ok_num,
           p_audio_render_data->show_frame_fail_num,
           p_audio_render_data->giveback_frame_ok_num,
           p_audio_render_data->giveback_frame_fail_num);
}


static void *mm_audio_render_component_thread(void *p_thread_data)
{
    s32 ret = MM_ERROR_NONE;
    s32 cmd = MM_COMMAND_UNKNOWN;
    MM_BOOL b_notify_frame_end = 0;

    mm_audio_render_data *p_audio_render_data =
        (mm_audio_render_data *)p_thread_data;
    mm_bind_info *p_bind_clock =
        &p_audio_render_data->in_port_bind[AUDIO_RENDER_PORT_IN_CLOCK_INDEX];
    mm_bind_info *p_bind_adec =
        &p_audio_render_data->in_port_bind[AUDIO_RENDER_PORT_IN_AUDIO_INDEX];
    p_audio_render_data->wait_ready_frame_flag = 1;

    while (1) {
    _AIC_MSG_GET_:

        /* process cmd and change state*/
        cmd = mm_audio_render_component_process_cmd(p_audio_render_data);
        if (MM_COMMAND_STATE_SET == cmd) {
            continue;
        } else if (MM_COMMAND_STOP == cmd) {
            goto _EXIT;
        }

        if (p_audio_render_data->state != MM_STATE_EXECUTING) {
            aic_msg_wait_new_msg(&p_audio_render_data->s_msg, 0);
            continue;
        }
        if (p_audio_render_data->flags &
            AUDIO_RENDER_INPORT_SEND_ALL_FRAME_FLAG) {
            if (!b_notify_frame_end) {
                mm_audio_render_event_notify(p_audio_render_data,
                                             MM_EVENT_BUFFER_FLAG, 0, 0, NULL);
                b_notify_frame_end = 1;
            }
            aic_msg_wait_new_msg(&p_audio_render_data->s_msg, 0);
            continue;
        }
        b_notify_frame_end = 0;

        /* get frame from audio decoder*/
        ret = mm_audio_render_get_frame(p_bind_adec->p_bind_comp,
                                        &p_audio_render_data->frame);
        if (ret != DEC_OK) {
            mm_audio_render_wait_frame_timeout(p_audio_render_data);
            continue;
        }

        p_audio_render_data->receive_frame_num++;

        if (p_audio_render_data->frame_fisrt_show_flag) {
            /* first frame do init render and set attr*/
            mm_audio_render_set_attr(p_audio_render_data);

            if (p_bind_clock->flag) { // set clock start time
                mm_time_config_timestamp timestamp;
                timestamp.port_index = p_bind_clock->port_index;
                timestamp.timestamp = p_audio_render_data->frame.pts;
                p_audio_render_data->pre_frame_pts =
                    p_audio_render_data->frame.pts;
                p_audio_render_data->first_frame_pts =
                    p_audio_render_data->frame.pts;
                mm_set_config(p_bind_clock->p_bind_comp,
                              MM_INDEX_CONFIG_TIME_CLIENT_START_TIME,
                              &timestamp);
                p_audio_render_data->pre_correct_media_time =
                    p_audio_render_data->frame.pts;
                // whether need to wait????
                if (p_audio_render_data->clock_state !=
                    MM_TIME_CLOCK_STATE_RUNNING) {
                    ret = mm_audio_render_put_frame(
                        p_bind_adec->p_bind_comp, &p_audio_render_data->frame);
                    if (ret != DEC_OK) {
                        p_audio_render_data->giveback_frame_fail_num++;
                    } else {
                        p_audio_render_data->giveback_frame_ok_num++;
                    }
                    mm_send_command(p_bind_adec->p_bind_comp, MM_COMMAND_WKUP,
                                    0, NULL);
                    aic_msg_wait_new_msg(&p_audio_render_data->s_msg,
                                         10 * 1000);
                    goto _AIC_MSG_GET_;
                }
                printf("[%s:%d]video start time arrive\n", __FUNCTION__,
                       __LINE__);
            }

            ret = 0;
            if (p_audio_render_data->frame.size > 0) { // frame size maybe 0.
                ret = p_audio_render_data->render->rend(
                    p_audio_render_data->render,
                    p_audio_render_data->frame.data,
                    p_audio_render_data->frame.size);
            }
            if (ret == 0) {
                p_audio_render_data->show_frame_ok_num++;
                p_audio_render_data->frame_fisrt_show_flag = MM_FALSE;
                mm_audio_render_event_notify(p_audio_render_data,
                                             MM_EVENT_VIDEO_RENDER_FIRST_FRAME,
                                             0, 0, NULL);
#ifdef AUDIO_RENDRE_DUMP_ENABLE
                mm_audio_render_dump(p_audio_render_data,
                                     p_audio_render_data->frame.data,
                                     p_audio_render_data->frame.size);
#endif
            } else {
                /*how to do ,video can deal with  same success,drop this frame,but audio can not drop it.*/
                loge("first frame error,there is something wrong!!!!\n");
                p_audio_render_data->show_frame_fail_num++;
            }
            ret = mm_audio_render_put_frame(p_bind_adec->p_bind_comp,
                                            &p_audio_render_data->frame);
            if (ret != DEC_OK) {
                p_audio_render_data->giveback_frame_fail_num++;
            } else {
                p_audio_render_data->giveback_frame_ok_num++;
            }
        } else { // not first frame
            if (p_audio_render_data->volume_change) {
                if (p_audio_render_data->render->set_volume(
                        p_audio_render_data->render,
                        p_audio_render_data->volume) == 0) {
                    p_audio_render_data->volume_change = 0;
                } else {
                    loge("set_volume error\n");
                }
            }
            mm_process_audio_sync(p_audio_render_data,
                                  &p_audio_render_data->frame);
            ret = 0;
            // last frame size maybe 0.
            if (p_audio_render_data->frame.size > 0) {
                ret = p_audio_render_data->render->rend(
                    p_audio_render_data->render,
                    p_audio_render_data->frame.data,
                    p_audio_render_data->frame.size);
            }
            if (ret == 0) {
                p_audio_render_data->show_frame_ok_num++;

#ifdef AUDIO_RENDRE_DUMP_ENABLE
                mm_audio_render_dump(p_audio_render_data,
                                     p_audio_render_data->frame.data,
                                     p_audio_render_data->frame.size);
#endif
                if (p_audio_render_data->frame.flag & FRAME_FLAG_EOS) {
                    p_audio_render_data->flags |=
                        AUDIO_RENDER_INPORT_SEND_ALL_FRAME_FLAG;
                    printf("[%s:%d]receive frame_end_flag\n", __FUNCTION__, __LINE__);
                }

                mm_audio_render_calc_frame_num(p_audio_render_data);
            } else {
                loge("frame erro!!!!\n");
                p_audio_render_data->show_frame_fail_num++;
            }

            ret = mm_audio_render_put_frame(p_bind_adec->p_bind_comp,
                                            &p_audio_render_data->frame);
            if (ret != DEC_OK) {
                p_audio_render_data->giveback_frame_fail_num++;
            } else {
                p_audio_render_data->giveback_frame_ok_num++;
            }
            mm_send_command(p_bind_adec->p_bind_comp, MM_COMMAND_WKUP, 0, NULL);
        }
    }

_EXIT:
    if (p_audio_render_data->render) {
        aic_audio_render_destroy(p_audio_render_data->render);
        p_audio_render_data->render = NULL;
    }
    mm_audio_render_frame_count_print(p_audio_render_data);
    printf("[%s:%d]mm_audio_render_component_thread exit\n",__FUNCTION__,
           __LINE__);
    return (void *)MM_ERROR_NONE;
}
