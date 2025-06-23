/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <che.jiang@artinchip.com>
*  Desc: matroska tag
*/


#include "matroska_tag.h"

/* If you add a tag here that is not in aic_codec_bmp_tags[]
   or aic_codec_wav_tags[], add it also to additional_audio_tags[]
   or additional_video_tags[] in matroskaenc.c */
const struct mkv_codec_tag  mkv_codec_tags[]={
    {"A_AAC"            , CODEC_ID_AAC},
    {"A_MPEG/L3"        , CODEC_ID_MP3},

    {"A_PCM/FLOAT/IEEE" , CODEC_ID_PCM_F32LE},
    {"A_PCM/FLOAT/IEEE" , CODEC_ID_PCM_F64LE},
    {"A_PCM/INT/BIG"    , CODEC_ID_PCM_S16BE},
    {"A_PCM/INT/BIG"    , CODEC_ID_PCM_S24BE},
    {"A_PCM/INT/BIG"    , CODEC_ID_PCM_S32BE},
    {"A_PCM/INT/LIT"    , CODEC_ID_PCM_S16LE},
    {"A_PCM/INT/LIT"    , CODEC_ID_PCM_S24LE},
    {"A_PCM/INT/LIT"    , CODEC_ID_PCM_S32LE},
    {"A_PCM/INT/LIT"    , CODEC_ID_PCM_U8},

    {"V_MJPEG"          , CODEC_ID_MJPEG},
    {"V_MPEG4/ISO/ASP"  , CODEC_ID_MPEG4},
    {"V_MPEG4/ISO/AP"   , CODEC_ID_MPEG4},
    {"V_MPEG4/ISO/SP"   , CODEC_ID_MPEG4},
    {"V_MPEG4/ISO/AVC"  , CODEC_ID_H264},

    {""                 , CODEC_ID_NONE}
};

const struct mkv_codec_tag webm_codec_tags[] = {
    {""                 , CODEC_ID_NONE}
};
