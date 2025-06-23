/*
 * Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *  author: <jun.ma@artinchip.com>
 *  Desc: aic_player
 */

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>

#include "mm_core.h"
#include "mpp_dec_type.h"
#include "mpp_log.h"
#include "mpp_mem.h"
#include "mpp_dec_type.h"
#include "aic_parser.h"
#include "aic_player.h"

#define AIC_PLAYER_STATE_IDLE                          0
#define AIC_PLAYER_STATE_INITIALIZED                   1
#define AIC_PLAYER_STATE_PREPARING                     2
#define AIC_PLAYER_STATE_PREPARED                      3
#define AIC_PLAYER_STATE_STARTED                       4
#define AIC_PLAYER_STATE_PLAYING                       5
#define AIC_PLAYER_STATE_PAUSED                        6
#define AIC_PLAYER_STATE_STOPPED                       7
#define AIC_PLAYER_STATE_PLAYBACK_COMPLETED            8

#define AIC_VIDEO 0x01
#define AIC_AUDIO 0x02

#define AIC_PLAYER_PREPARE_FORMAT_DETECTING 0
#define AIC_PLAYER_PREPARE_FORMAT_DETECTED 1
#define AIC_PLAYER_PREPARE_FORMAT_NOT_DETECTED 2

struct aic_player {
    event_handler event_handle;
    void* app_data;
    mm_handle demuxer_handle;
    mm_handle vdecoder_handle;
    mm_handle adecoder_handle;
    mm_handle video_render_handle;
    mm_handle audio_render_handle;
    mm_handle clock_handle;
    s32 format_detected;//0-dectecting , 1-dectected ,2- not_detected
    struct aic_parser_av_media_info media_info;
    u32 video_audio_end_mask;
    u32 video_audio_seek_mask;
    int state;
    struct mpp_rect disp_rect;
    u32 sync_flag;
    pthread_t threadId;
    s32 thread_runing;
    mm_param_content_uri * uri_param;
    s64 video_pts;
    s64 audio_pts;
    s32 volume;
    s32 rotation_angle;
    s8 mute;
    s8 seeking;
};


#define  wait_state(\
        h_component,\
        des_state)\
         {\
            MM_STATE_TYPE state;\
            while(1) {\
                mm_get_state(h_component, &state);\
                if (state == des_state) {\
                    break;\
                } else {\
                    usleep(1000);\
                }\
            }\
        }                /* Macro End */

#define _CLOCK_COMPONENT_

static s32 component_event_handler (
    mm_handle h_component,
    void* p_app_data,
    MM_EVENT_TYPE event,
    u32 data1,
    u32 data2,
    void* p_event_data)
{
    s32 error = MM_ERROR_NONE;
    struct aic_player *player = (struct aic_player *)p_app_data;
    //struct aic_parser_av_media_info sMediaInfo;

    switch((s32)event) {
        case MM_EVENT_CMD_COMPLETE:
            break;
        case MM_EVENT_BUFFER_FLAG:
            if (player->media_info.has_video) {
                if (player->video_render_handle == h_component) {
                    player->video_audio_end_mask &= ~AIC_VIDEO;
                    printf("[%s:%d]rececive video_render_end,video_audio_end_mask:%d!!!\n",__FUNCTION__,__LINE__,player->video_audio_end_mask);
                    if (player->video_audio_end_mask == 0) {
                        player->event_handle(player->app_data,AIC_PLAYER_EVENT_PLAY_END,0,0);
                        player->state = AIC_PLAYER_STATE_PLAYBACK_COMPLETED;
                        printf("[%s:%d]play end!!!\n",__FUNCTION__,__LINE__);
                    }
                }
            }
            if (player->media_info.has_audio) {
                if (player->audio_render_handle == h_component) {
                    player->video_audio_end_mask &= ~AIC_AUDIO;
                    printf("[%s:%d]rececive audio_render_handle,video_audio_end_mask:%d!!!\n",__FUNCTION__,__LINE__,player->video_audio_end_mask);
                    if (player->video_audio_end_mask == 0) {
                        player->event_handle(player->app_data,AIC_PLAYER_EVENT_PLAY_END,0,0);
                        player->state = AIC_PLAYER_STATE_PLAYBACK_COMPLETED;
                        printf("[%s:%d]play end!!!\n",__FUNCTION__,__LINE__);
                    }
                }
            }

            break;
        case MM_EVENT_PORT_FORMAT_DETECTED:
            printf("[%s:%d]MM_EVENT_PORT_FORMAT_DETECTED\n",__FUNCTION__,__LINE__);
            memcpy(&player->media_info,p_event_data,sizeof(struct aic_parser_av_media_info));
            player->format_detected = AIC_PLAYER_PREPARE_FORMAT_DETECTED;
            //player->event_handle(player->app_data,AIC_PLAYER_EVENT_DEMUXER_FORMAT_DETECTED,0,0);
            break;
        case MM_EVENT_ERROR:
            if (data1 == MM_ERROR_FORMAT_NOT_DETECTED) {
                player->format_detected = AIC_PLAYER_PREPARE_FORMAT_NOT_DETECTED;
               // player->event_handle(player->app_data,AIC_PLAYER_EVENT_DEMUXER_FORMAT_NOT_DETECTED,0,0);
            } else if (data1 == MM_ERROR_MB_ERRORS_IN_FRAME || data1 == MM_ERROR_INSUFFICIENT_RESOURCES) {
                player->event_handle(player->app_data,AIC_PLAYER_EVENT_PLAY_END,0,0);
                player->state = AIC_PLAYER_STATE_PLAYBACK_COMPLETED;
                printf("[%s:%d]play end!!!\n",__FUNCTION__,__LINE__);
            }
            break;
        case MM_EVENT_VIDEO_RENDER_PTS:
            player->video_pts = data1;
            player->video_pts = (player->video_pts << (sizeof(u32)*8));
            player->video_pts |= data2;
            //loge("video_pts:%lld\n",video_pts);
            break;
        case MM_EVENT_AUDIO_RENDER_PTS:
            player->audio_pts = data1;
            player->audio_pts = (player->audio_pts << (sizeof(u32)*8));
            player->audio_pts |= data2;
            player->event_handle(player->app_data,AIC_PLAYER_EVENT_PLAY_TIME,data1,data2);
            break;
        case MM_EVENT_VIDEO_RENDER_FIRST_FRAME:
        case MM_EVENT_AUDIO_RENDER_FIRST_FRAME:
            if (player->media_info.has_video) {
                if (player->video_render_handle == h_component) {
                    printf("[%s:%d]first video frame come!!!\n",__FUNCTION__,__LINE__);
                    player->video_audio_seek_mask &= ~AIC_VIDEO;
                    if (player->video_audio_seek_mask == 0) {
                        player->seeking = 0;
                    }
                }
            }
            if (player->media_info.has_audio) {
                if (player->audio_render_handle == h_component) {
                    printf("[%s:%d]first audio  frame come!!!\n",__FUNCTION__,__LINE__);
                    player->video_audio_seek_mask &= ~AIC_AUDIO;
                    if (player->video_audio_seek_mask == 0) {
                        player->seeking = 0;
                    }
                }
            }
            break;
        default:
            break;
    }
    return error;
}


mm_callback component_event_callbacks = {
    .event_handler    = component_event_handler
};

struct aic_player* aic_player_create(char *uri)
{
    s32 error;
    struct aic_player * player = mpp_alloc(sizeof(struct aic_player));

    if (player == NULL) {
        loge("mpp_alloc aic_player error\n");
        return NULL;
    }

    memset(player,0x00,sizeof(struct aic_player));

    player->uri_param = (mm_param_content_uri *)mpp_alloc(sizeof(mm_param_content_uri) + MM_MAX_STRINGNAME_SIZE);

    if (player->uri_param == NULL) {
        loge("mpp_alloc aic_player error\n");
        goto _exit;
    }

    error = mm_init();

    if (error != MM_ERROR_NONE) {
        loge("mm_init error!!!\n");
        goto _exit;
    }

    if (uri != NULL) {
        if (aic_player_set_uri(player,uri)) {
            loge("aic_player_set_uri error!!!\n");
            goto _exit;
        }
    } else {
        player->state = AIC_PLAYER_STATE_IDLE;
    }

    if (MM_ERROR_NONE !=mm_get_handle(&player->demuxer_handle, MM_COMPONENT_DEMUXER_NAME,player, &component_event_callbacks)) {
        loge("unable to get demuxer handle.\n");
        goto _exit;
    }

    return player;

_exit:
    if (player->uri_param) {
        mpp_free(player->uri_param);
        player->uri_param = NULL;
    }
    mpp_free(player);
    return NULL;
}

s32 aic_player_set_uri(struct aic_player *player,char *uri)
{
    int uri_len;

    if (uri == NULL) {
        loge("param  error\n");
        return -1;
    }
    if (player->uri_param  == NULL) {
        loge("player->uri_param=NULL\n");
        return -1;
    }
    uri_len = strlen(uri);
    if (uri_len > MM_MAX_STRINGNAME_SIZE-1) {
        loge("path too long\n");
        return -1;
    }
    memset(player->uri_param->content_uri,0x00,MM_MAX_STRINGNAME_SIZE);
    player->uri_param->size = sizeof(mm_param_content_uri) + uri_len;
    strcpy((char *)player->uri_param->content_uri,uri);
    player->state = AIC_PLAYER_STATE_INITIALIZED;
    return 0;
}

static void* player_index_param_content_uri_thread(void *pThreadData)
{
    struct aic_player *player = (struct aic_player *)pThreadData;
    player->format_detected = AIC_PLAYER_PREPARE_FORMAT_DETECTING;
    player->state = AIC_PLAYER_STATE_PREPARING;
    /*mm_set_parameter is blocking*/
    player->thread_runing = 1;
    mm_set_parameter(player->demuxer_handle, MM_INDEX_PARAM_CONTENT_URI, player->uri_param);
    if (player->format_detected != AIC_PLAYER_PREPARE_FORMAT_DETECTED) {
        loge("MM_ERROR_FORMAT_NOT_DETECTED !!!!");
         player->event_handle(player->app_data,AIC_PLAYER_EVENT_DEMUXER_FORMAT_NOT_DETECTED,0,0);
        return (void*)-1;
    } else {
        player->state = AIC_PLAYER_STATE_PREPARED;
        player->event_handle(player->app_data,AIC_PLAYER_EVENT_DEMUXER_FORMAT_DETECTED,0,0);
    }
    player->thread_runing = 0;
    return (void*)0;
}

s32 aic_player_prepare_async(struct aic_player *player)
{
    int ret = 0;
    if (player->state == AIC_PLAYER_STATE_PREPARING) {
        logw("player->state hase been in AIC_PLAYER_STATE_PREPARING \n");
        return 0;
    }
    if (NULL == player->demuxer_handle) {
        loge("player->demuxer_handle has not been created \n");
        return -1;
    }

    player->sync_flag = AIC_PLAYER_PREPARE_ASYNC;

    ret = pthread_create(&player->threadId, NULL, player_index_param_content_uri_thread, player);
    if (ret) {
        loge("pthread_create fail!");
        return -1;
    }
    return 0;
}


s32 aic_player_prepare_sync(struct aic_player *player)
{
    s32 error = MM_ERROR_NONE;
    if (player->state != AIC_PLAYER_STATE_INITIALIZED) {
        loge("player->state is not  in AIC_PLAYER_STATE_INITIALIZED,plaese set uri!!!\n");
        return -1;
    }

    if (NULL == player->demuxer_handle) {
        loge("player->demuxer_handle has not been created \n");
        return -1;
    }

    player->sync_flag = AIC_PLAYER_PREPARE_SYNC;
    player->format_detected = AIC_PLAYER_PREPARE_FORMAT_DETECTING;
    /*mm_set_parameter is blocking*/
    error = mm_set_parameter(player->demuxer_handle, MM_INDEX_PARAM_CONTENT_URI, player->uri_param);
    if ((error != MM_ERROR_NONE) || (player->format_detected != AIC_PLAYER_PREPARE_FORMAT_DETECTED)) {
        loge("MM_ERROR_FORMAT_NOT_DETECTED!!!!");
        return -1;
    } else {
        player->state = AIC_PLAYER_STATE_PREPARED;
    }
    return 0;
}

s32 aic_player_start_video(struct aic_player *player)
{
    mm_video_param_port_format video_port_format;

    if (player->media_info.has_video) {
        video_port_format.port_index = DEMUX_PORT_VIDEO_INDEX;
        video_port_format.index = 0;
        if (MM_ERROR_NONE != mm_get_parameter(player->demuxer_handle,
                                              MM_INDEX_PARAM_VIDEO_PORT_FORMAT,
                                              &video_port_format)) {
            loge("mm_get_parameter Error!!!!.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (player->vdecoder_handle) {
            loge("please call aic_player_stop,free(vdecoder_handle)\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_get_handle(&player->vdecoder_handle,
                                           MM_COMPONENT_VDEC_NAME, player,
                                           &component_event_callbacks)) {
            loge("unable to get vdecoder_handle handle.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        video_port_format.color_format = MM_COLOR_FORMAT_YUV420P;

        video_port_format.port_index = VDEC_PORT_IN_INDEX;

        if (MM_ERROR_NONE != mm_set_parameter(player->vdecoder_handle,
                                              MM_INDEX_PARAM_VIDEO_PORT_FORMAT,
                                              &video_port_format)) {
            mm_param_skip_track skip_track;
            loge("MM_INDEX_PARAM_VIDEO_PORT_FORMAT Error!!!!.\n");
            skip_track.port_index = DEMUX_PORT_VIDEO_INDEX;
            mm_set_parameter(player->demuxer_handle,
                             MM_INDEX_VENDOR_DEMUXER_SKIP_TRACK, &skip_track);
            mm_free_handle(player->vdecoder_handle);
            player->vdecoder_handle = NULL;
            player->media_info.has_video = 0;
            return MM_ERROR_BAD_PARAMETER;
        }

        if (player->video_render_handle) {
            loge("please call aic_player_stop,free(vdecoder_handle)\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_get_handle(&player->video_render_handle,
                                           MM_COMPONENT_VIDEO_RENDER_NAME,
                                           player,
                                           &component_event_callbacks)) {
            loge("unable to get video_render_handle handle.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_set_config(player->video_render_handle,
                                           MM_INDEX_VENDOR_VIDEO_RENDER_INIT,
                                           NULL)) {
            loge("mm_set_config Error!!!!.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (player->disp_rect.width != 0 && player->disp_rect.height != 0) {
            aic_player_set_disp_rect(player, &player->disp_rect);
        }
        if (player->rotation_angle != MPP_ROTATION_0) {
            aic_player_set_rotation(player, player->rotation_angle);
        }

        if (MM_ERROR_NONE !=
            mm_set_bind(player->demuxer_handle, DEMUX_PORT_VIDEO_INDEX,
                        player->vdecoder_handle, VDEC_PORT_IN_INDEX)) {
            loge("mm_set_bind error.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_set_bind(player->vdecoder_handle,
                                         VDEC_PORT_OUT_INDEX,
                                         player->video_render_handle,
                                         VIDEO_RENDER_PORT_IN_VIDEO_INDEX)) {
            loge("mm_set_bind error.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        player->video_audio_end_mask |= AIC_VIDEO;
    }

    return MM_ERROR_NONE;
}


s32 aic_player_start_audio(struct aic_player *player)
{
    mm_audio_param_port_format audio_port_format;

    if (player->media_info.has_audio) {
        audio_port_format.port_index = DEMUX_PORT_AUDIO_INDEX;
        audio_port_format.index = 0;

        if (MM_ERROR_NONE != mm_get_parameter(player->demuxer_handle,
                                              MM_INDEX_PARAM_AUDIO_PORT_FORMAT,
                                              &audio_port_format)) {
            loge("mm_get_parameter Error!!!!.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (player->adecoder_handle) {
            loge("please call aic_player_stop,free(adecoder_handle)\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_get_handle(&player->adecoder_handle,
                                           MM_COMPONENT_ADEC_NAME, player,
                                           &component_event_callbacks)) {
            loge("unable to get adecoder_handle handle.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        audio_port_format.port_index = ADEC_PORT_IN_INDEX;

        if (MM_ERROR_NONE != mm_set_parameter(player->adecoder_handle,
                                              MM_INDEX_PARAM_AUDIO_PORT_FORMAT,
                                              &audio_port_format)) {
            mm_param_skip_track skip_track;
            loge("MM_INDEX_PARAM_AUDIO_PORT_FORMAT Error!!!!.\n");
            skip_track.port_index = DEMUX_PORT_AUDIO_INDEX;
            mm_set_parameter(player->demuxer_handle,
                             MM_INDEX_VENDOR_DEMUXER_SKIP_TRACK, &skip_track);
            mm_free_handle(player->adecoder_handle);
            player->adecoder_handle = NULL;
            player->media_info.has_audio = 0;
            return MM_ERROR_BAD_PARAMETER;
        }

        if (player->audio_render_handle) {
            loge("please call aic_player_stop,free(audio_render_handle)\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_get_handle(&player->audio_render_handle,
                                           MM_COMPONENT_AUDIO_RENDER_NAME,
                                           player,
                                           &component_event_callbacks)) {
            loge("unable to get audio_render_handle handle.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_set_config(player->audio_render_handle,
                                           MM_INDEX_VENDOR_AUDIO_RENDER_INIT,
                                           NULL)) {
            loge("mm_set_config error!!!!.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (player->volume != 0) {
            aic_player_set_volum(player, player->volume);
        }

        if (MM_ERROR_NONE !=
            mm_set_bind(player->demuxer_handle, DEMUX_PORT_AUDIO_INDEX,
                        player->adecoder_handle, ADEC_PORT_IN_INDEX)) {
            loge("mm_set_bind error.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_set_bind(player->adecoder_handle,
                                         ADEC_PORT_OUT_INDEX,
                                         player->audio_render_handle,
                                         AUDIO_RENDER_PORT_IN_AUDIO_INDEX)) {
            loge("mm_set_bind error.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        player->video_audio_end_mask |= AIC_AUDIO;
    }

    return MM_ERROR_NONE;
}


s32 aic_player_start_clock(struct aic_player *player)
{
#ifdef _CLOCK_COMPONENT_
    if (player->media_info.has_video && player->media_info.has_audio) {
        mm_time_config_clock_state clock_state;

        if (player->clock_handle) {
            loge("please call aic_player_stop,free(clock_handle)\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_get_handle(&player->clock_handle,
                                           MM_COMPONENT_CLOCK_NAME, player,
                                           &component_event_callbacks)) {
            loge("unable to get clock_handle handle.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_set_bind(player->clock_handle,
                                         CLOCK_PORT_OUT_VIDEO,
                                         player->video_render_handle,
                                         VIDEO_RENDER_PORT_IN_CLOCK_INDEX)) {
            loge("unable to get video_render handle.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }

        if (MM_ERROR_NONE != mm_set_bind(player->clock_handle,
                                         CLOCK_PORT_OUT_AUDIO,
                                         player->audio_render_handle,
                                         AUDIO_RENDER_PORT_IN_CLOCK_INDEX)) {
            loge("unable to get video_render handle.\n");
            return MM_ERROR_INSUFFICIENT_RESOURCES;
        }
        memset(&clock_state, 0x00, sizeof(mm_time_config_clock_state));
        clock_state.state = MM_TIME_CLOCK_STATE_WAITING_FOR_START_TIME;
        clock_state.wait_mask |= MM_CLOCK_PORT0;
        clock_state.wait_mask |= MM_CLOCK_PORT1;
        mm_set_config(player->clock_handle, MM_INDEX_CONFIG_TIME_CLOCK_STATE,
                      &clock_state);
    }
#endif

    return MM_ERROR_NONE;
}

s32 aic_player_start(struct aic_player *player)
{
    s32 ret = MM_ERROR_NONE;

    if (player->state == AIC_PLAYER_STATE_STARTED) {
        loge("player->state has been  in AIC_PLAYER_STATE_STARTED \n");
        return 0;
    }

    if (player->state != AIC_PLAYER_STATE_PREPARED) {
        loge("player->state is not in AIC_PLAYER_STATE_PREPARED ,it can not do this opt\n");
        return -1;
    }

    if (player->sync_flag == AIC_PLAYER_PREPARE_ASYNC) {
        if (player->threadId != 0) {
            printf("[%s:%d]wait pthread_join\n",__FUNCTION__,__LINE__);
            pthread_join(player->threadId, NULL);
            printf("[%s:%d]pthread_join ok\n",__FUNCTION__,__LINE__);
            player->threadId = 0;
        }
    }

    ret = aic_player_start_video(player);
    if ((MM_ERROR_INSUFFICIENT_RESOURCES == ret) || (MM_ERROR_UNSUPPORT == ret)) {
        goto _EXIT;
    }

    ret = aic_player_start_audio(player);
    if (MM_ERROR_INSUFFICIENT_RESOURCES == ret) {
        goto _EXIT;
    }

    ret = aic_player_start_clock(player);
    if (MM_ERROR_INSUFFICIENT_RESOURCES == ret) {
        goto _EXIT;
    }

    if (!player->vdecoder_handle && !player->adecoder_handle) {
        loge("video and audio all  do not support !!!!.\n");
        goto _EXIT;
    }
#ifdef _CLOCK_COMPONENT_
    if (player->clock_handle) {
        mm_send_command(player->clock_handle, MM_COMMAND_STATE_SET,
                        MM_STATE_IDLE, NULL);
    }
#endif
    if (player->media_info.has_video && player->video_render_handle &&
        player->vdecoder_handle) {
        mm_send_command(player->video_render_handle, MM_COMMAND_STATE_SET,
                        MM_STATE_IDLE, NULL);
        mm_send_command(player->vdecoder_handle, MM_COMMAND_STATE_SET,
                        MM_STATE_IDLE, NULL);
    }
    if (player->media_info.has_audio && player->audio_render_handle &&
        player->adecoder_handle) {
        mm_send_command(player->audio_render_handle, MM_COMMAND_STATE_SET,
                        MM_STATE_IDLE, NULL);
        mm_send_command(player->adecoder_handle, MM_COMMAND_STATE_SET,
                        MM_STATE_IDLE, NULL);
    }
    if (player->demuxer_handle) {
        mm_send_command(player->demuxer_handle, MM_COMMAND_STATE_SET,
                        MM_STATE_IDLE, NULL);
    }
    player->state = AIC_PLAYER_STATE_STARTED;

    if (aic_player_play(player)) {
        loge("aic_player_play fail !!!!.\n");
        goto _EXIT;
    }

    return 0;

_EXIT:

#ifdef _CLOCK_COMPONENT_
    if (player->clock_handle) {
        mm_free_handle(player->clock_handle);
        player->clock_handle = NULL;
    }
#endif

    if (player->audio_render_handle) {
        mm_free_handle(player->audio_render_handle);
        player->audio_render_handle = NULL;
    }

    if (player->adecoder_handle) {
        mm_free_handle(player->adecoder_handle);
        player->adecoder_handle = NULL;
    }

    if (player->video_render_handle) {
        mm_free_handle(player->video_render_handle);
        player->video_render_handle = NULL;
    }

    if (player->vdecoder_handle) {
        mm_free_handle(player->vdecoder_handle);
        player->vdecoder_handle = NULL;
    }

    return -1;
}

s32 aic_player_play(struct aic_player *player)
{

    if (player->state == AIC_PLAYER_STATE_PLAYING) {
        logi("it is already in AIC_PLAYER_STATE_PLAYING\n");
        return 0;
    }
    if (player->state != AIC_PLAYER_STATE_STARTED && player->state != AIC_PLAYER_STATE_PAUSED) {
        loge("player->state:[%d] in AIC_PLAYER_STATE_STARTED or AIC_PLAYER_STATE_PAUSED ,it can not do this opt\n",player->state);
        return -1;
    }

#ifdef _CLOCK_COMPONENT_
        if (player->clock_handle) {
            mm_send_command(player->clock_handle, MM_COMMAND_STATE_SET, MM_STATE_EXECUTING, NULL);
        }
#endif

    if (player->media_info.has_video && player->video_render_handle) {
        mm_send_command(player->video_render_handle, MM_COMMAND_STATE_SET, MM_STATE_EXECUTING, NULL);
    }
    if (player->media_info.has_audio && player->audio_render_handle) {
        mm_send_command(player->audio_render_handle, MM_COMMAND_STATE_SET, MM_STATE_EXECUTING, NULL);
    }
    if (player->media_info.has_video && player->vdecoder_handle) {
        mm_send_command(player->vdecoder_handle, MM_COMMAND_STATE_SET, MM_STATE_EXECUTING, NULL);
    }
    if (player->media_info.has_audio && player->adecoder_handle) {
        mm_send_command(player->adecoder_handle, MM_COMMAND_STATE_SET, MM_STATE_EXECUTING, NULL);
    }
    if (player->demuxer_handle) {
        mm_send_command(player->demuxer_handle, MM_COMMAND_STATE_SET, MM_STATE_EXECUTING, NULL);
    }

    player->state = AIC_PLAYER_STATE_PLAYING;

    return 0;
}

s32 aic_player_pause(struct aic_player *player)
{
    if (player->state == AIC_PLAYER_STATE_PAUSED) {
        logi("it is already in AIC_PLAYER_STATE_PAUSED\n");
        return aic_player_play(player);
    } else if (player->state != AIC_PLAYER_STATE_PLAYING && player->state != AIC_PLAYER_STATE_PLAYBACK_COMPLETED) {
        loge("player->state:[%d] in AIC_PLAYER_STATE_STARTED or AIC_PLAYER_STATE_PAUSED ,it can not do this opt\n",player->state);
        return -1;
    }
    if (player->media_info.has_audio && player->audio_render_handle && player->adecoder_handle) {
        mm_send_command(player->audio_render_handle, MM_COMMAND_STATE_SET, MM_STATE_PAUSE, NULL);
        wait_state(player->audio_render_handle,MM_STATE_PAUSE);
        mm_send_command(player->adecoder_handle, MM_COMMAND_STATE_SET, MM_STATE_PAUSE, NULL);
        wait_state(player->adecoder_handle,MM_STATE_PAUSE);
    }
    if (player->media_info.has_video && player->video_render_handle && player->vdecoder_handle) {
        mm_send_command(player->video_render_handle, MM_COMMAND_STATE_SET, MM_STATE_PAUSE, NULL);
        wait_state(player->video_render_handle,MM_STATE_PAUSE);
        mm_send_command(player->vdecoder_handle, MM_COMMAND_STATE_SET, MM_STATE_PAUSE, NULL);
        wait_state(player->vdecoder_handle,MM_STATE_PAUSE);
    }
    if (player->demuxer_handle) {
        mm_send_command(player->demuxer_handle, MM_COMMAND_STATE_SET, MM_STATE_PAUSE, NULL);
        wait_state(player->demuxer_handle,MM_STATE_PAUSE);
    }
#ifdef _CLOCK_COMPONENT_
    if (player->clock_handle) {
        mm_send_command(player->clock_handle, MM_COMMAND_STATE_SET, MM_STATE_PAUSE, NULL);
        wait_state(player->clock_handle,MM_STATE_PAUSE);
    }
#endif
        player->state = AIC_PLAYER_STATE_PAUSED;
    return 0;
}

static int do_seek(struct aic_player *player,u64 seek_time)
{
    mm_time_config_timestamp  time_stamp;
    player->seeking = 1;
    time_stamp.timestamp = seek_time;
    if (MM_ERROR_NONE !=  mm_set_config(player->demuxer_handle,MM_INDEX_CONFIG_TIME_POSITION,&time_stamp)) {
        goto _exit;
    }

    player->video_audio_seek_mask = 0;

    if (player->media_info.has_video && player->video_render_handle && player->vdecoder_handle) {
        if(MM_ERROR_NONE !=  mm_set_config(player->video_render_handle,MM_INDEX_CONFIG_TIME_POSITION,&time_stamp)) {
            goto _exit;
        }
        if (MM_ERROR_NONE !=  mm_set_config(player->vdecoder_handle,MM_INDEX_CONFIG_TIME_POSITION,&time_stamp)) {
            goto _exit;
        }
        player->video_audio_seek_mask |= AIC_VIDEO;
        player->video_audio_end_mask |= AIC_VIDEO;

    }

    if (player->media_info.has_audio && player->audio_render_handle && player->adecoder_handle) {
        if(MM_ERROR_NONE !=  mm_set_config(player->audio_render_handle,MM_INDEX_CONFIG_TIME_POSITION,&time_stamp)) {
            goto _exit;
        }
        if (MM_ERROR_NONE !=  mm_set_config(player->adecoder_handle,MM_INDEX_CONFIG_TIME_POSITION,&time_stamp)) {
            goto _exit;
        }
        player->video_audio_seek_mask |= AIC_AUDIO;
        player->video_audio_end_mask |= AIC_AUDIO;
    }

    if (player->media_info.has_video && player->media_info.has_audio && player->clock_handle) {
        if(MM_ERROR_NONE !=  mm_set_config(player->clock_handle,MM_INDEX_CONFIG_TIME_POSITION,&time_stamp)) {
            goto _exit;
        }
    }

    if (MM_ERROR_NONE !=  mm_set_config(player->demuxer_handle,MM_INDEX_VENDOR_CLEAR_BUFFER,&time_stamp)) {
            goto _exit;
    }
    return 0;

_exit:
    loge("seek error!\n");
    player->seeking = 0;
    player->video_audio_seek_mask = 0;
    return -1;

}

s32 aic_player_seek(struct aic_player *player,u64 seek_time)
{
    int ret = 0;
    mm_time_config_timestamp  time_stamp;
    if (player->seeking) {
        loge("palyer in seeking\n");
        return -1;
    }
    if ((player->state == AIC_PLAYER_STATE_PREPARED) || (player->state == AIC_PLAYER_STATE_STARTED)) {
        time_stamp.timestamp = seek_time;
        //logd("time_stamp.timestamp:"FMT_d64"\n",time_stamp.timestamp);
        player->seeking = 1;
        if (MM_ERROR_NONE !=  mm_set_config(player->demuxer_handle,MM_INDEX_CONFIG_TIME_POSITION,&time_stamp)) {
            loge("seek error!\n");
            player->seeking = 0;
            ret = -1;
        }
    } else if ((player->state == AIC_PLAYER_STATE_PLAYING) || (player->state == AIC_PLAYER_STATE_PLAYBACK_COMPLETED)) {
        aic_player_pause(player);
        ret = do_seek(player,seek_time);
        if (ret != 0) {
            loge("seek error!\n");
            ret = -1;
        } else {
            aic_player_play(player);
        }
    } else if (player->state == AIC_PLAYER_STATE_PAUSED) {
        ret = do_seek(player,seek_time);
        if (ret != 0) {
            loge("seek error!\n");
            ret = -1;
        } else {
            aic_player_play(player);
        }
    } else {
        return -1;
    }

    return ret;
}

void aic_player_stop_component(struct aic_player *player)
{
    if (player->media_info.has_video) {
        if (player->video_render_handle) {
            mm_send_command(player->video_render_handle, MM_COMMAND_STATE_SET, MM_STATE_IDLE, NULL);
            wait_state(player->video_render_handle,MM_STATE_IDLE);
            mm_send_command(player->video_render_handle, MM_COMMAND_STATE_SET, MM_STATE_LOADED, NULL);
            wait_state(player->video_render_handle,MM_STATE_LOADED);
        }
        if (player->vdecoder_handle) {
            mm_send_command(player->vdecoder_handle, MM_COMMAND_STATE_SET, MM_STATE_IDLE, NULL);
            wait_state(player->vdecoder_handle,MM_STATE_IDLE);
            mm_send_command(player->vdecoder_handle, MM_COMMAND_STATE_SET, MM_STATE_LOADED, NULL);
            wait_state(player->vdecoder_handle,MM_STATE_LOADED);
        }
    }

    if (player->media_info.has_audio) {
        if (player->audio_render_handle) {
            mm_send_command(player->audio_render_handle, MM_COMMAND_STATE_SET, MM_STATE_IDLE, NULL);
            wait_state(player->audio_render_handle,MM_STATE_IDLE);
            mm_send_command(player->audio_render_handle, MM_COMMAND_STATE_SET, MM_STATE_LOADED, NULL);
            wait_state(player->audio_render_handle,MM_STATE_LOADED);
        }
        if (player->adecoder_handle) {
            mm_send_command(player->adecoder_handle, MM_COMMAND_STATE_SET, MM_STATE_IDLE, NULL);
            wait_state(player->adecoder_handle,MM_STATE_IDLE);
            mm_send_command(player->adecoder_handle, MM_COMMAND_STATE_SET, MM_STATE_LOADED, NULL);
            wait_state(player->adecoder_handle,MM_STATE_LOADED);
        }
    }

    if (player->demuxer_handle) {
        mm_send_command(player->demuxer_handle, MM_COMMAND_STATE_SET, MM_STATE_IDLE, NULL);
        wait_state(player->demuxer_handle,MM_STATE_IDLE);
        mm_send_command(player->demuxer_handle, MM_COMMAND_STATE_SET, MM_STATE_LOADED, NULL);
        wait_state(player->demuxer_handle,MM_STATE_LOADED);
    }

#ifdef _CLOCK_COMPONENT_
    if (player->media_info.has_video && player->media_info.has_audio) {
        if (player->clock_handle) {
            mm_send_command(player->clock_handle, MM_COMMAND_STATE_SET, MM_STATE_IDLE, NULL);
            mm_send_command(player->clock_handle, MM_COMMAND_STATE_SET, MM_STATE_LOADED, NULL);
        }
    }
#endif

    if (player->media_info.has_video) {
        if (player->demuxer_handle && player->vdecoder_handle && player->video_render_handle) {
            mm_set_bind(player->demuxer_handle,DEMUX_PORT_VIDEO_INDEX,NULL,0);
            mm_set_bind(NULL,0,player->vdecoder_handle,VDEC_PORT_IN_INDEX);
            mm_set_bind(player->vdecoder_handle,VDEC_PORT_OUT_INDEX,NULL,0);
            mm_set_bind(NULL,0,player->video_render_handle,VIDEO_RENDER_PORT_IN_VIDEO_INDEX);
        }
    }

    if (player->media_info.has_audio) {
        if (player->demuxer_handle && player->adecoder_handle && player->audio_render_handle) {
            mm_set_bind(player->demuxer_handle,DEMUX_PORT_AUDIO_INDEX,NULL,0);
            mm_set_bind(NULL,0,player->adecoder_handle,ADEC_PORT_IN_INDEX);
            mm_set_bind(player->adecoder_handle,ADEC_PORT_OUT_INDEX,NULL,0);
            mm_set_bind(NULL,0,player->audio_render_handle,AUDIO_RENDER_PORT_IN_AUDIO_INDEX);
        }
    }

#ifdef _CLOCK_COMPONENT_
    if (player->media_info.has_video && player->media_info.has_audio) {
        if (player->clock_handle && player->video_render_handle && player->audio_render_handle) {
            mm_set_bind(player->clock_handle,CLOCK_PORT_OUT_VIDEO,NULL,0);
            mm_set_bind(NULL,0,player->audio_render_handle,AUDIO_RENDER_PORT_IN_CLOCK_INDEX);
            mm_set_bind(player->clock_handle,CLOCK_PORT_OUT_AUDIO,NULL,0);
            mm_set_bind(NULL,0,player->video_render_handle,VIDEO_RENDER_PORT_IN_CLOCK_INDEX);
        }
    }
#endif
}


s32 aic_player_stop(struct aic_player *player)
{
    if (player->state == AIC_PLAYER_STATE_STOPPED) {
         loge("player->state has been  in AIC_PLAYER_STATE_STOPPED \n");
         return 0;
    }

    if (player->sync_flag == AIC_PLAYER_PREPARE_ASYNC) {
        if (player->threadId != 0) {
            printf("[%s:%d]pthread_cancel,player->thread_runing:%d\n",__FUNCTION__,__LINE__,player->thread_runing);
            if (player->thread_runing == 1) {
                printf("[%s:%d]pthread_cancel\n",__FUNCTION__,__LINE__);
                pthread_cancel(player->threadId);
            }
            printf("[%s:%d]wait   pthread_join\n",__FUNCTION__,__LINE__);
            pthread_join(player->threadId, NULL);
            printf("[%s:%d]pthread_join ok\n",__FUNCTION__,__LINE__);
            player->threadId = 0;
        }
    }

    aic_player_stop_component(player);

    player->video_audio_end_mask = 0;

    if (player->media_info.has_video) {
        if (player->video_render_handle) {
            mm_free_handle(player->video_render_handle);
            player->video_render_handle = NULL;
        }
        if (player->vdecoder_handle) {
            mm_free_handle(player->vdecoder_handle);
            player->vdecoder_handle = NULL;
        }
    }

    if (player->media_info.has_audio) {
        if (player->audio_render_handle) {
            mm_free_handle(player->audio_render_handle);
            player->audio_render_handle = NULL;
        }
        if (player->adecoder_handle) {
            mm_free_handle(player->adecoder_handle);
            player->adecoder_handle = NULL;
        }
    }

#ifdef _CLOCK_COMPONENT_
    if (player->media_info.has_video && player->media_info.has_audio) {
        if (player->clock_handle) {
            mm_free_handle(player->clock_handle);
            player->clock_handle = NULL;
        }

    }
#endif

    memset(&player->media_info,0x00,sizeof(struct aic_parser_av_media_info));
    player->state = AIC_PLAYER_STATE_STOPPED;
    player->seeking = 0;
    return 0;
}

s32 aic_player_destroy(struct aic_player *player)
{
    if (player->state != AIC_PLAYER_STATE_STOPPED) {
        aic_player_stop(player);
    }

    if (player->demuxer_handle) {
        mm_free_handle(player->demuxer_handle);
        player->demuxer_handle = NULL;
    }

    if (player->uri_param) {
        mpp_free(player->uri_param);
        player->uri_param = NULL;
    }

    mpp_free(player);

    mm_deinit();

    return 0;
}

s32 aic_player_set_event_callback(struct aic_player *player,void* app_data,event_handler event_handle)
{
    player->event_handle = event_handle;
    player->app_data = app_data;
    return 0;
}

s32 aic_player_get_media_info(struct aic_player *player,struct av_media_info *media_info)
{
    if (media_info == NULL) {
        return -1;
    }
    if ((!player->media_info.has_video) && (!player->media_info.has_audio)) {
        return -1;
    }
    media_info->duration = player->media_info.duration;
    media_info->file_size = player->media_info.file_size;
    media_info->has_video = player->media_info.has_video;
    media_info->has_audio = player->media_info.has_audio;
    if (media_info->has_video) {
        media_info->video_stream.width = player->media_info.video_stream.width;
        media_info->video_stream.height = player->media_info.video_stream.height;
    }
    if (media_info->has_audio) {
        media_info->audio_stream.bits_per_sample = player->media_info.audio_stream.bits_per_sample;
        media_info->audio_stream.nb_channel = player->media_info.audio_stream.nb_channel;
        media_info->audio_stream.sample_rate = player->media_info.audio_stream.sample_rate;
    }
    return 0;
}

s32 aic_player_get_screen_size(struct aic_player *player,struct mpp_size *screen_size)
{
    mm_param_screen_size rect = {0};

    if (!player->media_info.has_video || !player->video_render_handle) {
        loge("no video!!!!\n");
        return -1;
    }

    mm_get_parameter(player->video_render_handle, MM_INDEX_VENDOR_VIDEO_RENDER_SCREEN_SIZE, &rect);
    screen_size->width = rect.width;
    screen_size->height = rect.height;
    return 0;
}

s32 aic_player_set_disp_rect(struct aic_player *player,struct mpp_rect *disp_rect)
{
    mm_config_rect rect = {0};

    player->disp_rect = *disp_rect;
    if (!player->media_info.has_video || !player->video_render_handle) {
        return 0;
    }
    rect.left = disp_rect->x;
    rect.top = disp_rect->y;
    rect.width = disp_rect->width;
    rect.height = disp_rect->height;
    mm_set_parameter(player->video_render_handle, MM_INDEX_CONFIG_COMMON_OUTPUT_CROP, &rect);
    return 0;
}


s32 aic_player_get_disp_rect(struct aic_player *player,struct mpp_rect *disp_rect)
{
    mm_config_rect rect;

    if (!player->media_info.has_video || !player->video_render_handle) {
        loge("no video!!!!\n");
        return -1;
    }
    mm_get_parameter(player->video_render_handle, MM_INDEX_CONFIG_COMMON_OUTPUT_CROP, &rect);
    disp_rect->x = rect.left;
    disp_rect->y = rect.top;
    disp_rect->width = rect.width;
    disp_rect->height = rect.height;
    player->disp_rect = *disp_rect;
    return 0;
}


s64 aic_player_get_play_time(struct aic_player *player)
{
    //to do
    if (player->media_info.has_audio) {
        return player->audio_pts;
    } else if (player->media_info.has_video) {
        return player->video_pts;
    } else {
        return -1;
    }
}

s32 aic_player_set_mute(struct aic_player *player)
{
    mm_param_audio_volume volume;
    if (!player->media_info.has_audio || !player->audio_render_handle) {
        return -1;
    }
    if (player->mute) {
        volume.volume = player->volume;
        player->mute = 0;
    } else {
        player->mute = 1;
        volume.volume = 0;
    }
    mm_set_parameter(player->audio_render_handle, MM_INDEX_VENDOR_AUDIO_RENDER_VOLUME, &volume);
    return 0;
}

s32 aic_player_set_volum(struct aic_player *player,s32 vol)
{
    mm_param_audio_volume volume;
    player->volume = vol;
    if (!player->media_info.has_audio || !player->audio_render_handle) {
        return 0;
    }
    volume.volume = vol;
    mm_set_parameter(player->audio_render_handle, MM_INDEX_VENDOR_AUDIO_RENDER_VOLUME, &volume);
    return 0;
}

s32 aic_player_get_volum(struct aic_player *player,s32 *vol)
{
    mm_param_audio_volume volume = {0};
    if (!player->media_info.has_audio || !player->audio_render_handle || !vol) {
        return -1;
    }

    mm_get_parameter(player->video_render_handle, MM_INDEX_VENDOR_AUDIO_RENDER_VOLUME, &volume);
    *vol = volume.volume;
    player->volume = volume.volume;
    return 0;
}

#define BYTE_ALIGN(x, byte) (((x) + ((byte) - 1))&(~((byte) - 1)))
s32 aic_player_capture(struct aic_player *player, struct aic_capture_info *capture_info)
{
    mm_param_video_capture capture;
    if (!player->media_info.has_video || !player->video_render_handle) {
        loge("no video!!!!\n");
        return -1;
    }
    if (player->state != AIC_PLAYER_STATE_PAUSED) {
        loge("not in AIC_PLAYER_STATE_PAUSED!!!!\n");
        return -1;
    }

    capture.p_file_path = (s8 *)capture_info->file_path;
    capture.width = BYTE_ALIGN(capture_info->width, 16);
    capture.height = BYTE_ALIGN(capture_info->height, 16);
    capture.quality = capture_info->quality;
    if (capture.quality < 1 || capture.quality > 100) {
        capture.quality = 80;
    }
    if (MM_ERROR_NONE != mm_set_config(player->video_render_handle, MM_INDEX_VENDOR_VIDEO_RENDER_CAPTURE, &capture)) {
        loge("no video!!!!\n");
        return -1;
    }
    return 0;
}

s32 aic_player_set_rotation(struct aic_player *player, int rotation_angle)
{
    mm_config_rotation rotation;

    if (rotation_angle != MPP_ROTATION_0
        && rotation_angle != MPP_ROTATION_90
        && rotation_angle != MPP_ROTATION_180
        && rotation_angle != MPP_ROTATION_270) {
            loge("param error!!!!\n");
            return -1;
    }
    player->rotation_angle = rotation_angle;
    if (!player->media_info.has_video || !player->video_render_handle) {
        return 0;
    }
    rotation.rotation = rotation_angle;
    if (MM_ERROR_NONE != mm_set_config(player->video_render_handle,MM_INDEX_CONFIG_COMMON_ROTATE,&rotation)) {
        loge("no video!!!!\n");
        return -1;
    }
    return 0;
}

s32 aic_player_get_rotation(struct aic_player *player)
{
    mm_config_rotation rotation = {0};
    if (!player->media_info.has_video || !player->video_render_handle) {
        loge("no video!!!!\n");
        return -1;
    }

    if (MM_ERROR_NONE != mm_get_config(player->video_render_handle,MM_INDEX_CONFIG_COMMON_ROTATE,&rotation)) {
        loge("no video!!!!\n");
        return -1;
    }
    player->rotation_angle = rotation.rotation;
    return rotation.rotation;
}

