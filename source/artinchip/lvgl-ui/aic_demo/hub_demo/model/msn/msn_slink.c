/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  artinchip
 */

#ifdef USE_MSNLINK
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "msn_slink.h"
#include "msn_sink_audio.h"
#include "msn_sink_video.h"

bool gBTConnectType = 1;
bool gUSBIsConnectType = 0;
int  gConnType = 2;
LinkType  gStartLinkType = LINK_TYPE_CARLIFE;

static int link_status;
/**
   * @brief Video start callback
   */
void sink_video_start(LinkType type)
{
    printf("%s %d\n", __FUNCTION__, __LINE__);
    aic_video_start(type);
}

/**
   * @brief Video frame recv callback
   * @param datas [in] h264 video frame datas
   * @param len[in] frame data len
   * @param idrFrame[in] is idr frame(has sps nal and pps nal)
   */
void sink_video_play(LinkType type, void * datas, int len, bool idrFrame)
{
    aic_video_play(type, datas,len,idrFrame);
}

/**
   * @brief Video stop callback
   */
void sink_video_stop(LinkType type)
{
    printf("%s %d\n", __FUNCTION__, __LINE__);
    aic_video_stop(type);
}

/**
   * @brief Audio start callback
   * @param streamType [in] audio stream type
   * @param audioType [in] audio type
   * @param rate [in] audio sample rate
   * @param format[in] audio format usually is 16 (PCM_FORMAT_S16_LE)
   * @param channel[in] audio channel
   */
void sink_audio_start(LinkType type, int streamType, int audioType, int rate, int format, int channel)
{
    printf("%s streamType:%d audioType:%d rate:%d format:%d channel:%d\n",
           __FUNCTION__, streamType, audioType, rate, format, channel);
    aic_audio_start(type, streamType, audioType, rate, format, channel);
}

/**
   * @brief Audio frame recv callback
   * @param streamType [in] audio stream type
   * @param audioType [in] audio type
   * @param datas [in] pcm frame datas
   * @param len[in] frame data len (is not frame size)
   */
void sink_audio_play(LinkType type, int streamType, int audioType, void * datas, int len)
{
    aic_audio_play(type, streamType, audioType, datas, len);
}

/**
   * @brief Audio stop callback
   * @param streamType [in] audio stream type
   * @param audioType [in] audio type
   */
void sink_audio_stop(LinkType type, int streamType, int audioType)
{
    printf("%s streamType:%d audioType:%d\n", __FUNCTION__, streamType, audioType);
    aic_audio_stop(type, streamType, audioType);
}

/**
   * @brief mic input start callback
   * @param rate [in] audio sample rate
   * @param format[in] audio format usually is 16 (PCM_FORMAT_S16_LE)
   * @param channel[in] audio channel
   * @note use @see send_microphone_datas to write mic datas
   */
void sink_micinput_start(LinkType type, int rate, int format, int channel)
{
    printf("%s %d\n", __FUNCTION__, __LINE__);
}

/**
   * @brief mic input stop callback
   */
void sink_micinput_stop(LinkType type)
{
    printf("%s %d\n", __FUNCTION__, __LINE__);
}

/**
   * @brief Link state notify
   * @param linkType[in] @see LinkType
   * @param type[in] @see SINK_NOTIFY_XXX define
   * @param lparam [in] reserved, action param @see SINK_NOTIFY_XXX
   * @param sparam [in] reserved, action param @see SINK_NOTIFY_XXX
   */
void sink_notify(LinkType linkType, int eventType, int64_t lparam, const char * sparam)
{
    printf("%s link type:%d eventType:%x lparam:%ld\n", __FUNCTION__, linkType, eventType, lparam);

    if (SINK_NOTIFY_SDK_INITED == eventType)
    {
        // set dev name
        request_link_action(LINK_TYPE_UNKOW, LINK_ACTION_SET_DEVICE_NAME, 0, "Link");
        // set back icon name
        request_link_action(LINK_TYPE_UNKOW, LINK_ACTION_SET_OEM_LABEL, 0, "AutoLink");
        // set hotpoint name that equal to hostapd.conf
        request_link_action(LINK_TYPE_UNKOW, LINK_ACTION_SET_HOTSPOT_NAME, 0, "carplay");
        // set hotpoint password that equal to hostapd.conf
        request_link_action(LINK_TYPE_UNKOW, LINK_ACTION_SET_HOTSPOT_PSK, 0, "88888888");

        // start internet services
        start_link_service(gStartLinkType);
    } else if (SINK_NOTIFY_SERVICE_STARTED == eventType) {
        link_status = SLINK_UNCONNECTED;
    } else if (SINK_NOTIFY_SERVICE_STOPED == eventType) {
        link_status = SLINK_UNCONNECTED;
    } else if (SINK_NOTIFY_PHONE_CONNECTING == eventType) {
        link_status = SLINK_CONNECTING;
    } else if (SINK_NOTIFY_PHONE_CONNECTED == eventType) {
        link_status = SLINK_CONNECTED;
        printf("\nlink service:%d phone connected\n", linkType);
    } else if (SINK_NOTIFY_PHONE_CONNECTION_FAILED == eventType) {

    } else if (SINK_NOTIFY_PHONE_DISCONNECTED == eventType) {
        link_status = SLINK_UNCONNECTED;
        printf("\nlink service:%d phone disconnected\n", linkType);
    } else if (SINK_NOTIFY_USB_CONNECT_STATE_CHANGE == eventType) {
        // USB hot plug notification
        printf("----------- connect type = %ld \n", lparam);
        link_status = SLINK_CONNECTING;
        gUSBIsConnectType = lparam;
    if (lparam == 1) {
        // start connecting phone
        request_link_action(LINK_TYPE_CARPLAY, LINK_ACTION_START_PHONE_CONNECTION, 1, 0);
    } else if (lparam == 2) {
        request_link_action(LINK_TYPE_CARPLAY, LINK_ACTION_START_PHONE_CONNECTION, 1, 0);
    } else if (lparam == 5) {
        request_link_action(LINK_TYPE_CARLIFE, LINK_ACTION_START_PHONE_CONNECTION, 1, 0);
    }
    } else if (SINK_NOTIFY_PHONE_CONNECTION_READY == eventType) {
    } else if (SINK_NOTIFY_VIDEO_CONTENT_SIZE == eventType) {
    // set display area
        // uint16_t width = (lparam >> 16) & 0xFFFF;
        // uint16_t height = (lparam) & 0xFFFF;
    } else if (SINK_NOTIFY_GOTO_HOME == eventType) {
        link_status = SLINK_UNCONNECTED;
        request_link_action(linkType, LINK_ACTION_VIDEO_CTRL, 0, NULL);
    }
}

void slink_set_gbtc_connect_type(bool btc_connect_type)
{
    gBTConnectType = btc_connect_type;
}

void slink_set_gusb_is_connect_type(bool usb_is_connect_type)
{
    gUSBIsConnectType = usb_is_connect_type;
}

void slink_set_gstart_link_type(int start_link)
{
    if (start_link > LINK_TYPE_UNKOW && start_link < LINK_TYPE_END) {
        gStartLinkType = (LinkType)start_link;
    }
}

void slink_set_gconnect_type(int connect_type)
{
    gConnType = connect_type;
}

struct msn_slink* msn_slink_create(LinkType connect_type)
{
    struct msn_slink *link = (struct msn_slink *)malloc(sizeof(struct msn_slink));
    if (link == NULL) {
        return NULL;
    }

    if (connect_type > LINK_TYPE_UNKOW && connect_type <= LINK_TYPE_END) {
        gStartLinkType = (LinkType)connect_type;
    }

    link_player_sink *player_link = &link->sink;
    player_link->audio_play = sink_audio_play;
    player_link->audio_start = sink_audio_start;
    player_link->audio_stop = sink_audio_stop;

    player_link->video_start = sink_video_start;
    player_link->video_play = sink_video_play;
    player_link->video_stop = sink_video_stop;

    player_link->micinput_start = sink_micinput_start;
    player_link->micinput_stop = sink_micinput_stop;

    player_link->sink_notify = sink_notify;

    link->link_status = SLINK_UNCONNECTED;

    return link;
}

int msn_slink_modify_connect_type(struct msn_slink* link, LinkType connect_type)
{
    if (link == NULL) {
        return -1;
    }

    if (connect_type > LINK_TYPE_UNKOW && connect_type < LINK_TYPE_END) {
        gStartLinkType = (LinkType)connect_type;
    }

    printf("gStartLinkType = %d\n", gStartLinkType);
    return 0;
}

int msn_slink_get_status(struct msn_slink* link)
{
    if (link == NULL) {
        return -1;
    }

    link->link_status = link_status;

    return link->link_status;
}

int msn_slink_start(struct msn_slink* link)
{
    if (link == NULL) {
        return -1;
    }

    init_link_sdk(link->sink, "00:88:22:23:73:2e", 1024, 600, 0, 0);

    printf("gStartLinkType = %d\n", gStartLinkType);
    return start_link_service(gStartLinkType);
}

int msn_slink_stop(struct msn_slink* link)
{
    if (link == NULL) {
        return -1;
    }

    link->link_status = SLINK_UNCONNECTED;
    return stop_link_service(gStartLinkType);
}

int msn_slink_delete(struct msn_slink* link)
{
    uninit_link_sdk();

    if (link) {
        free(link);
    }

    return 0;
}
#endif

