/*
* Copyright (C) 2020-2022 Artinchip Technology Co. Ltd
*
*  author: <qi.xu@artinchip.com>
*  Desc:
*/

#ifndef MPP_VIDEO_H
#define MPP_VIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mpp_decoder.h"

struct mpp_video;

struct mpp_video* mpp_video_create();

int mpp_video_init(struct mpp_video* video, enum mpp_codec_type type, struct decode_config *config);

int mpp_video_start(struct mpp_video* video);

int mpp_video_set_disp_window(struct mpp_video* video, struct mpp_rect* disp_window);

int mpp_video_send_packet(struct mpp_video* p, struct mpp_packet* data, int timeout_ms);

int mpp_video_stop(struct mpp_video* video);

void mpp_video_destroy(struct mpp_video* video);

#ifdef __cplusplus
}
#endif

#endif
