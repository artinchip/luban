/*
 * Copyright (C) 2022-2023 ArtinChip Technology Co., Ltd.
 * Authors:  Ning Fang <ning.fang@artinchip.com>
 */

#ifndef AIC_UI_H
#define AIC_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define CONN(x, y) x#y
#define LVGL_PATH(y) CONN(LVGL_DIR, y)

#define FILE_LIST_PATH  "/usr/local/share/lvgl_data/"
#define LVGL_FILE_LIST_PATH(y) CONN(FILE_LIST_PATH, y)

/* use fake image to fill color */
#define FAKE_IMAGE_DECLARE(name) char fake_##name[128];
#define FAKE_IMAGE_INIT(name, w, h, blend, color) \
                snprintf(fake_##name, 128, "L:/%dx%d_%d_%08x.fake",\
                 w, h, blend, color);
#define FAKE_IMAGE_NAME(name) (fake_##name)
#define FAKE_IMAGE_PARSE(fake_name, ret, width, height, blend, color) \
        ret = sscanf(fake_name + 3, "%dx%d_%d_%08x", &width, &height, &blend, &color);

uint32_t custom_tick_get(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif //AIC_UI_H
