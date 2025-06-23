/*
* Copyright (C) 2020-2023 ArtInChip Technology Co. Ltd
*
*  author: <che.jiang@artinchip.com>
*  Desc: matriska tag
*/

#ifndef __MATROSKA_TAG__
#define __MATROSKA_TAG__

#include "aic_tag.h"

typedef struct mkv_codec_tag {
    char str[22];
    enum CodecID id;
}mkv_codec_tag;

extern const struct mkv_codec_tag mkv_codec_tags[];
extern const struct mkv_codec_tag webm_codec_tags[];


#endif /* __MATROSKA_TAG__ */
