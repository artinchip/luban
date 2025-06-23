/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: middle media clock component
 */

#include "mm_clock_component.h"

#include <pthread.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <inttypes.h>

#include "mpp_log.h"
#include "mpp_list.h"
#include "mpp_mem.h"
#include "aic_message.h"

#define CLOCK_PORT_NUM_MAX 2

typedef struct mm_clock_data {
    MM_STATE_TYPE state;
    pthread_mutex_t state_lock;
    mm_callback *p_callback;
    void *p_app_data;
    mm_handle h_self;

    mm_port_param port_param;
    mm_param_port_def out_port_def[2];
    mm_bind_info out_port_bind[2];

    struct aic_message_queue s_msg;

    mm_time_config_clock_state clock_state;
    mm_time_config_active_ref_clock active_ref_clock;
    s64 port_start_time[CLOCK_PORT_NUM_MAX];

    s64 ref_clock_time_base; //unit us
    s64 wall_time_base;
    s64 pause_time_point;
    s64 pause_time_durtion;

} mm_clock_data;

static s32
mm_clock_index_config_time_position(mm_handle h_component,
                                    mm_time_config_timestamp *p_timestamp)
{
    mm_clock_data *p_clock_data =
        (mm_clock_data *)(((mm_component *)h_component)->p_comp_private);
    p_clock_data->clock_state.state =
        MM_TIME_CLOCK_STATE_WAITING_FOR_START_TIME;
    p_clock_data->clock_state.wait_mask |= (MM_CLOCK_PORT0 | MM_CLOCK_PORT1);
    logd("mm_clock_index_config_time_position\n");
    return MM_ERROR_NONE;
}

static s32 mm_clock_get_parameter(mm_handle h_component, MM_INDEX_TYPE index,
                                  void *p_param)
{
    s32 error = MM_ERROR_NONE;
    mm_clock_data *p_clock_data;

    p_clock_data =
        (mm_clock_data *)(((mm_component *)h_component)->p_comp_private);

    switch (index) {
        case MM_INDEX_PARAM_PORT_DEFINITION: {
            mm_param_port_def *port = (mm_param_port_def *)p_param;
            if (port->port_index == CLOCK_PORT_OUT_VIDEO) {
                memcpy(port, &p_clock_data->out_port_def[CLOCK_PORT_OUT_VIDEO],
                       sizeof(mm_param_port_def));
            } else if (port->port_index == CLOCK_PORT_OUT_AUDIO) {
                memcpy(port, &p_clock_data->out_port_def[CLOCK_PORT_OUT_AUDIO],
                       sizeof(mm_param_port_def));
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

static s32 mm_clock_set_parameter(mm_handle h_component, MM_INDEX_TYPE index,
                                  void *p_param)
{
    s32 error = MM_ERROR_NONE;
    if (p_param == NULL) {
        loge("param error!!!\n");
        return MM_ERROR_BAD_PARAMETER;
    }
    switch (index) {
        case MM_INDEX_PARAM_PORT_DEFINITION:
            break;
        default:
            break;
    }
    return error;
}

static s64 mm_clock_get_system_time()
{
    struct timespec ts = { 0, 0 };
    s64 tick = 0;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    tick = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return tick;
}

static s32
mm_clock_config_time_cur_audio_ref(mm_handle h_component,
                                   mm_time_config_timestamp *p_timestamp)
{
    s64 cur_media_time;
    s64 diff_time;
    mm_clock_data *p_clock_data;
    p_clock_data =
        (mm_clock_data *)(((mm_component *)h_component)->p_comp_private);
    if (p_clock_data->clock_state.state != MM_TIME_CLOCK_STATE_RUNNING) {
        loge(
            "clockState are not in MM_TIME_CLOCK_STATE_STOPPED,do not set!!!\n");
        p_timestamp->timestamp = -1;
        return MM_ERROR_UNDEFINED;
    }

    cur_media_time =
        (mm_clock_get_system_time() - p_clock_data->wall_time_base -
         p_clock_data->pause_time_durtion) +
        p_clock_data->ref_clock_time_base;

    diff_time = cur_media_time - p_timestamp->timestamp;

    if (diff_time > 10 * 1000 || diff_time < -10 * 1000) { //10ms
        p_clock_data->ref_clock_time_base = p_timestamp->timestamp;
        p_clock_data->wall_time_base = mm_clock_get_system_time();
        p_clock_data->pause_time_durtion = 0;
    }

    return MM_ERROR_NONE;
}

static s32
mm_clock_get_current_media_time(mm_handle h_component,
                                mm_time_config_timestamp *p_timestamp)
{
    mm_clock_data *p_clock_data;
    p_clock_data =
        (mm_clock_data *)(((mm_component *)h_component)->p_comp_private);
    if (p_clock_data->clock_state.state != MM_TIME_CLOCK_STATE_RUNNING) {
        //loge("clockState are not in MM_TIME_CLOCK_STATE_RUNNING,do not get media time!!!\n");
        p_timestamp->timestamp = -1;
        return MM_ERROR_UNDEFINED;
    }
    //s64 tick = mm_clock_get_system_time();
    p_timestamp->timestamp =
        (mm_clock_get_system_time() - p_clock_data->wall_time_base -
         p_clock_data->pause_time_durtion) +
        p_clock_data->ref_clock_time_base;

    return MM_ERROR_NONE;
}

static s32
mm_clock_config_time_clock_state(mm_handle h_component,
                                 mm_time_config_clock_state *p_clock_state)
{
    mm_clock_data *p_clock_data;
    p_clock_data =
        (mm_clock_data *)(((mm_component *)h_component)->p_comp_private);
    if (p_clock_data->clock_state.state != MM_TIME_CLOCK_STATE_STOPPED) {
        loge(
            "clockState are not in MM_TIME_CLOCK_STATE_STOPPED,do not set!!!\n");
        return MM_ERROR_UNDEFINED;
    }
    memcpy(&p_clock_data->clock_state, p_clock_state,
           sizeof(mm_time_config_clock_state));
    printf("[%s:%d]wait_mask:0x%x,clock_state:%d\n", __FUNCTION__, __LINE__,
           p_clock_data->clock_state.wait_mask,
           p_clock_data->clock_state.state);
    //p_clock_data->clock_state.state = MM_TIME_CLOCK_STATE_WAITING_FOR_START_TIME;

    return MM_ERROR_NONE;
}

static s32
mm_clock_config_time_client_start_time(mm_handle h_component,
                                       mm_time_config_timestamp *p_timestamp)
{
    mm_clock_data *p_clock_data;
    //int i = 0;
    s64 mitimestamp;
    p_clock_data =
        (mm_clock_data *)(((mm_component *)h_component)->p_comp_private);
    mm_bind_info *p_video_bind =
        &p_clock_data->out_port_bind[CLOCK_PORT_OUT_VIDEO];
    mm_bind_info *p_audio_bind =
        &p_clock_data->out_port_bind[CLOCK_PORT_OUT_AUDIO];

    if (p_clock_data->clock_state.state !=
        MM_TIME_CLOCK_STATE_WAITING_FOR_START_TIME) {
        logw(
            "clockState are not in MM_TIME_CLOCK_STATE_WAITING_FOR_START_TIME,do not set!!!\n");
        return MM_ERROR_UNDEFINED;
    }

    if (p_clock_data->clock_state.wait_mask) {
        logd("port_index:%d,timestamp:" FMT_d64 "\n", p_timestamp->port_index,
             p_timestamp->timestamp);
        if (p_timestamp->port_index == CLOCK_PORT_OUT_VIDEO) {
            p_clock_data->clock_state.wait_mask &= ~MM_CLOCK_PORT0;
            p_clock_data->port_start_time[CLOCK_PORT_OUT_VIDEO] =
                p_timestamp->timestamp;
            logd("CLOCK_PORT_OUT_VIDEO wait_mask:0x%x,timestamp:" FMT_d64 "\n",
                 p_clock_data->clock_state.wait_mask, p_timestamp->timestamp);
        } else if (p_timestamp->port_index == CLOCK_PORT_OUT_AUDIO) {
            p_clock_data->clock_state.wait_mask &= ~MM_CLOCK_PORT1;
            p_clock_data->port_start_time[CLOCK_PORT_OUT_AUDIO] =
                p_timestamp->timestamp;
            logd("CLOCK_PORT_OUT_AUDIO wait_mask:0x%x,timestamp:" FMT_d64 "\n",
                 p_clock_data->clock_state.wait_mask, p_timestamp->timestamp);
        } else {
            return MM_ERROR_BAD_PARAMETER;
        }
    }

    if (!p_clock_data->clock_state.wait_mask) { //all port start time come
        mitimestamp = p_clock_data->port_start_time[0];
	#if 0
        for (i = 1; i < CLOCK_PORT_NUM_MAX; i++) {
            if (p_clock_data->port_start_time[i] < mitimestamp) {
                mitimestamp = p_clock_data->port_start_time[i];
            }
        }
	#else
        mitimestamp = p_clock_data->port_start_time[CLOCK_PORT_OUT_AUDIO];
	#endif
        p_clock_data->clock_state.start_time = mitimestamp;
        p_clock_data->ref_clock_time_base = mitimestamp;
        p_clock_data->wall_time_base = mm_clock_get_system_time();
        p_clock_data->pause_time_durtion = 0;
        p_clock_data->clock_state.state = MM_TIME_CLOCK_STATE_RUNNING;
        printf("[%s:%d]ref_clock_time_base:" FMT_d64 ",wall_time_base:" FMT_d64
               "\n",
               __FUNCTION__, __LINE__, p_clock_data->ref_clock_time_base,
               p_clock_data->wall_time_base);
        mm_set_config(p_video_bind->p_bind_comp,
                      MM_INDEX_CONFIG_TIME_CLOCK_STATE,
                      &p_clock_data->clock_state);
        mm_set_config(p_audio_bind->p_bind_comp,
                      MM_INDEX_CONFIG_TIME_CLOCK_STATE,
                      &p_clock_data->clock_state);
    }
    return MM_ERROR_NONE;
}

static s32 mm_clock_get_config(mm_handle h_component, MM_INDEX_TYPE index,
                               void *p_config)
{
    s32 error = MM_ERROR_NONE;

    if (p_config == NULL) {
        loge("param error!!!\n");
        return MM_ERROR_BAD_PARAMETER;
    }
    switch (index) {
        case MM_INDEX_CONFIG_TIME_CUR_MEDIA_TIME:
            error = mm_clock_get_current_media_time(
                h_component, (mm_time_config_timestamp *)p_config);
            break;
        default:
            break;
    }

    return error;
}

static s32 mm_clock_set_config(mm_handle h_component, MM_INDEX_TYPE index,
                               void *p_config)
{
    s32 error = MM_ERROR_NONE;

    if (p_config == NULL) {
        loge("param error!!!\n");
        return MM_ERROR_BAD_PARAMETER;
    }
    switch (index) {
        case MM_INDEX_CONFIG_TIME_CUR_AUDIO_REFERENCE:
            error = mm_clock_config_time_cur_audio_ref(
                h_component, (mm_time_config_timestamp *)p_config);
            break;

        case MM_INDEX_CONFIG_TIME_CLIENT_START_TIME:
            error = mm_clock_config_time_client_start_time(
                h_component, (mm_time_config_timestamp *)p_config);
            break;

        case MM_INDEX_CONFIG_TIME_POSITION: // do seek
            error = mm_clock_index_config_time_position(
                h_component, (mm_time_config_timestamp *)p_config);
            break;

        case MM_INDEX_CONFIG_TIME_CLOCK_STATE:
            error = mm_clock_config_time_clock_state(
                h_component, (mm_time_config_clock_state *)p_config);
            break;
        default:
            break;
    }
    return error;
}

static s32 mm_clock_get_state(mm_handle h_component, MM_STATE_TYPE *p_state)
{
    mm_clock_data *p_clock_data;
    s32 error = MM_ERROR_NONE;
    p_clock_data =
        (mm_clock_data *)(((mm_component *)h_component)->p_comp_private);

    pthread_mutex_lock(&p_clock_data->state_lock);
    *p_state = p_clock_data->state;
    pthread_mutex_unlock(&p_clock_data->state_lock);
    return error;
}

static s32 mm_clock_bind_request(mm_handle h_comp, u32 port,
                                 mm_handle h_bind_comp, u32 bind_port)
{
    s32 error = MM_ERROR_NONE;
    mm_param_port_def *p_port;
    mm_bind_info *p_bind_info;
    mm_clock_data *p_clock_data;
    p_clock_data = (mm_clock_data *)(((mm_component *)h_comp)->p_comp_private);
    if (p_clock_data->state != MM_STATE_LOADED) {
        loge(
            "Component is not in MM_STATE_LOADED,it is in%d,it can not tunnel\n",
            p_clock_data->state);
        return MM_ERROR_INVALID_STATE;
    }

    if (port == CLOCK_PORT_OUT_VIDEO) {
        p_port = &p_clock_data->out_port_def[CLOCK_PORT_OUT_VIDEO];
        p_bind_info = &p_clock_data->out_port_bind[CLOCK_PORT_OUT_VIDEO];
    } else if (port == CLOCK_PORT_OUT_AUDIO) {
        p_port = &p_clock_data->out_port_def[CLOCK_PORT_OUT_AUDIO];
        p_bind_info = &p_clock_data->out_port_bind[CLOCK_PORT_OUT_AUDIO];
    } else {
        loge("component can not find \n");
        return MM_ERROR_BAD_PARAMETER;
    }

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
        mm_param_port_def bind_port_def;
        bind_port_def.port_index = bind_port;
        mm_get_parameter(h_bind_comp, MM_INDEX_PARAM_PORT_DEFINITION,
                         &bind_port_def);

        if (bind_port_def.dir != MM_DIR_OUTPUT) {
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

static s32 mm_clock_set_callback(mm_handle h_component, mm_callback *p_cb,
                                 void *p_app_data)
{
    s32 error = MM_ERROR_NONE;
    mm_clock_data *p_clock_data;
    p_clock_data =
        (mm_clock_data *)(((mm_component *)h_component)->p_comp_private);
    p_clock_data->p_callback = p_cb;
    p_clock_data->p_app_data = p_app_data;
    return error;
}

s32 mm_clock_component_deinit(mm_handle h_component)
{
    s32 error = MM_ERROR_NONE;
    mm_component *p_comp;
    mm_clock_data *p_clock_data;
    p_comp = (mm_component *)h_component;

    p_clock_data = (mm_clock_data *)p_comp->p_comp_private;

    pthread_mutex_lock(&p_clock_data->state_lock);
    if (p_clock_data->state != MM_STATE_LOADED) {
        loge(
            "compoent is in %d,but not in MM_STATE_LOADED(1),can ont FreeHandle.\n",
            p_clock_data->state);
        pthread_mutex_unlock(&p_clock_data->state_lock);
        return MM_ERROR_UNSUPPORT;
    }
    pthread_mutex_unlock(&p_clock_data->state_lock);

    pthread_mutex_destroy(&p_clock_data->state_lock);

    aic_msg_destroy(&p_clock_data->s_msg);

    mpp_free(p_clock_data);
    p_clock_data = NULL;

    logi("mm_clock_component_deinit\n");
    return error;
}

static void mm_clock_event_notify(mm_clock_data *p_clock_data,
                                  MM_EVENT_TYPE event, u32 data1, u32 data2,
                                  void *p_event_data)
{
    if (p_clock_data && p_clock_data->p_callback &&
        p_clock_data->p_callback->event_handler) {
        p_clock_data->p_callback->event_handler(p_clock_data->h_self,
                                                p_clock_data->p_app_data, event,
                                                data1, data2, p_event_data);
    }
}

static void mm_clock_state_change_to_invalid(mm_clock_data *p_clock_data)
{
    p_clock_data->state = MM_STATE_INVALID;
    mm_clock_event_notify(p_clock_data, MM_EVENT_ERROR, MM_ERROR_INVALID_STATE,
                          0, NULL);
    mm_clock_event_notify(p_clock_data, MM_EVENT_CMD_COMPLETE,
                          MM_COMMAND_STATE_SET, p_clock_data->state, NULL);
}

static void mm_clock_state_change_to_loaded(mm_clock_data *p_clock_data)
{
    //int ret;
    if (p_clock_data->state == MM_STATE_IDLE) {
    } else if (p_clock_data->state == MM_STATE_EXECUTING) {
    } else if (p_clock_data->state == MM_STATE_PAUSE) {
    } else {
        mm_clock_event_notify(p_clock_data, MM_EVENT_ERROR,
                              MM_ERROR_INCORRECT_STATE_TRANSITION,
                              p_clock_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_clock_data->state = MM_STATE_LOADED;
    mm_clock_event_notify(p_clock_data, MM_EVENT_CMD_COMPLETE,
                          MM_COMMAND_STATE_SET, p_clock_data->state, NULL);
}

static void mm_clock_state_change_to_idle(mm_clock_data *p_clock_data)
{
    //int ret;
    if (p_clock_data->state == MM_STATE_LOADED) {
    } else if (p_clock_data->state == MM_STATE_PAUSE) {
    } else if (p_clock_data->state == MM_STATE_EXECUTING) {
    } else {
        mm_clock_event_notify(p_clock_data, MM_EVENT_ERROR,
                              MM_ERROR_INCORRECT_STATE_TRANSITION,
                              p_clock_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_clock_data->state = MM_STATE_IDLE;
    mm_clock_event_notify(p_clock_data, MM_EVENT_CMD_COMPLETE,
                          MM_COMMAND_STATE_SET, p_clock_data->state, NULL);
}

static void mm_clock_state_change_to_excuting(mm_clock_data *p_clock_data)
{
    if (p_clock_data->state == MM_STATE_LOADED) {
        mm_clock_event_notify(p_clock_data, MM_EVENT_ERROR,
                              MM_ERROR_INCORRECT_STATE_TRANSITION,
                              p_clock_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    } else if (p_clock_data->state == MM_STATE_IDLE) {
    } else if (p_clock_data->state == MM_STATE_PAUSE) {
        s64 cur_media_time;
        p_clock_data->pause_time_durtion +=
            (mm_clock_get_system_time() - p_clock_data->pause_time_point);
        printf("[%s:%d]mm_clock_get_system_time:" FMT_d64
               ",pause_time_point:" FMT_d64 ",pause_time_durtion:" FMT_d64
               ",wall_time_base:" FMT_d64 ",ref_clock_time_base:" FMT_d64 "\n",
               __FUNCTION__, __LINE__, mm_clock_get_system_time(),
               p_clock_data->pause_time_point, p_clock_data->pause_time_durtion,
               p_clock_data->wall_time_base, p_clock_data->ref_clock_time_base);

        cur_media_time =
            (mm_clock_get_system_time() - p_clock_data->wall_time_base -
             p_clock_data->pause_time_durtion) +
            p_clock_data->ref_clock_time_base;
        printf("[%s:%d]p_clock_data->pause_time_durtion:" FMT_d64
               ",cur_media_time:" FMT_d64 "\n",
               __FUNCTION__, __LINE__, p_clock_data->pause_time_durtion,
               cur_media_time);
    } else {
        mm_clock_event_notify(p_clock_data, MM_EVENT_ERROR,
                              MM_ERROR_INCORRECT_STATE_TRANSITION,
                              p_clock_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_clock_data->state = MM_STATE_EXECUTING;
    mm_clock_event_notify(p_clock_data, MM_EVENT_CMD_COMPLETE,
                          MM_COMMAND_STATE_SET, p_clock_data->state, NULL);
}

static void mm_clock_state_change_to_pause(mm_clock_data *p_clock_data)
{
    if (p_clock_data->state == MM_STATE_LOADED) {
        mm_clock_event_notify(p_clock_data, MM_EVENT_ERROR,
                              MM_ERROR_INCORRECT_STATE_TRANSITION,
                              p_clock_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;

    } else if (p_clock_data->state == MM_STATE_IDLE) {
    } else if (p_clock_data->state == MM_STATE_EXECUTING) {
        s64 cur_media_time;
        printf("[%s:%d]mm_clock_get_system_time:" FMT_d64
               ",pause_time_point:" FMT_d64 ",pause_time_durtion:" FMT_d64
               ",wall_time_base:" FMT_d64 ",ref_clock_time_base:" FMT_d64 "\n",
               __FUNCTION__, __LINE__, mm_clock_get_system_time(),
               p_clock_data->pause_time_point, p_clock_data->pause_time_durtion,
               p_clock_data->wall_time_base, p_clock_data->ref_clock_time_base);
        cur_media_time =
            (mm_clock_get_system_time() - p_clock_data->wall_time_base -
             p_clock_data->pause_time_durtion) +
            p_clock_data->ref_clock_time_base;
        printf("[%s:%d]mm_clock_get_system_time:" FMT_d64
               ",pause_time_point:" FMT_d64 ",pause_time_durtion:" FMT_d64
               ",wall_time_base:" FMT_d64 ",ref_clock_time_base:" FMT_d64
               ",cur_media_time:" FMT_d64 "\n",
               __FUNCTION__, __LINE__, mm_clock_get_system_time(),
               p_clock_data->pause_time_point, p_clock_data->pause_time_durtion,
               p_clock_data->wall_time_base, p_clock_data->ref_clock_time_base,
               cur_media_time);

        p_clock_data->pause_time_point = mm_clock_get_system_time();
    } else {
        mm_clock_event_notify(p_clock_data, MM_EVENT_ERROR,
                              MM_ERROR_INCORRECT_STATE_TRANSITION,
                              p_clock_data->state, NULL);
        loge("MM_ERROR_INCORRECT_STATE_TRANSITION\n");
        return;
    }
    p_clock_data->state = MM_STATE_PAUSE;
    mm_clock_event_notify(p_clock_data, MM_EVENT_CMD_COMPLETE,
                          MM_COMMAND_STATE_SET, p_clock_data->state, NULL);
}

/*
    there is on need to create a pthread to run this component.
    processing cmd directly in mm_clock_send_command.
*/
static s32 mm_clock_send_command(mm_handle h_component, MM_COMMAND_TYPE cmd,
                                 u32 param1, void *p_cmd_data)
{
    mm_clock_data *p_clock_data;
    s32 error = MM_ERROR_NONE;
    //struct aic_message sMsg;
    p_clock_data =
        (mm_clock_data *)(((mm_component *)h_component)->p_comp_private);

    if (MM_COMMAND_STATE_SET == cmd) {
        pthread_mutex_lock(&p_clock_data->state_lock);
        if (p_clock_data->state == (MM_STATE_TYPE)(param1)) {
            logi("MM_ERROR_SAME_STATE\n");
            mm_clock_event_notify(p_clock_data, MM_EVENT_ERROR,
                                  MM_ERROR_SAME_STATE, 0, NULL);
            pthread_mutex_unlock(&p_clock_data->state_lock);
            return MM_ERROR_SAME_STATE;
        }
        switch (param1) {
            case MM_STATE_INVALID:
                mm_clock_state_change_to_invalid(p_clock_data);
                break;
            case MM_STATE_LOADED:
                mm_clock_state_change_to_loaded(p_clock_data);
                break;
            case MM_STATE_IDLE:
                mm_clock_state_change_to_idle(p_clock_data);
                break;
            case MM_STATE_EXECUTING:
                mm_clock_state_change_to_excuting(p_clock_data);
                break;
            case MM_STATE_PAUSE:
                mm_clock_state_change_to_pause(p_clock_data);
                break;
            default:
                break;
        }
        pthread_mutex_unlock(&p_clock_data->state_lock);
    }

    return error;
}

s32 mm_clock_component_init(mm_handle h_component)
{
    mm_component *p_comp;
    mm_clock_data *p_clock_data;
    s32 error = MM_ERROR_NONE;

    logi("mm_clock_component_init....\n");

    p_comp = (mm_component *)h_component;

    p_clock_data = (mm_clock_data *)mpp_alloc(sizeof(mm_clock_data));

    if (NULL == p_clock_data) {
        loge("mpp_alloc(sizeof(mm_clock_data) fail!");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT1;
    }

    memset(p_clock_data, 0x0, sizeof(mm_clock_data));
    p_comp->p_comp_private = (void *)p_clock_data;
    p_clock_data->state = MM_STATE_LOADED;
    p_clock_data->h_self = p_comp;

    p_comp->set_callback = mm_clock_set_callback;
    p_comp->send_command = mm_clock_send_command;
    p_comp->get_state = mm_clock_get_state;
    p_comp->get_parameter = mm_clock_get_parameter;
    p_comp->set_parameter = mm_clock_set_parameter;
    p_comp->get_config = mm_clock_get_config;
    p_comp->set_config = mm_clock_set_config;
    p_comp->bind_request = mm_clock_bind_request;
    p_comp->deinit = mm_clock_component_deinit;

    p_clock_data->out_port_def[CLOCK_PORT_OUT_VIDEO].port_index =
        CLOCK_PORT_OUT_VIDEO;
    p_clock_data->out_port_def[CLOCK_PORT_OUT_VIDEO].enable = MM_TRUE;
    p_clock_data->out_port_def[CLOCK_PORT_OUT_VIDEO].dir = MM_DIR_OUTPUT;

    p_clock_data->out_port_def[CLOCK_PORT_OUT_AUDIO].port_index =
        CLOCK_PORT_OUT_AUDIO;
    p_clock_data->out_port_def[CLOCK_PORT_OUT_AUDIO].enable = MM_TRUE;
    p_clock_data->out_port_def[CLOCK_PORT_OUT_AUDIO].dir = MM_DIR_OUTPUT;

    p_clock_data->clock_state.state = MM_TIME_CLOCK_STATE_STOPPED;
    p_clock_data->clock_state.start_time = -1;
    for (s32 i = 0; i < CLOCK_PORT_NUM_MAX; i++) {
        p_clock_data->port_start_time[i] = -1;
    }
    p_clock_data->active_ref_clock.clock = MM_TIME_REF_CLOCK_AUDIO;

    p_clock_data->pause_time_durtion = 0;

    if (aic_msg_create(&p_clock_data->s_msg) < 0) {
        loge("aic_msg_create fail!");
        error = MM_ERROR_INSUFFICIENT_RESOURCES;
        goto _EXIT2;
    }

    pthread_mutex_init(&p_clock_data->state_lock, NULL);

    return error;

_EXIT2:
    if (p_clock_data) {
        mpp_free(p_clock_data);
        p_clock_data = NULL;
    }

_EXIT1:
    return error;
}
