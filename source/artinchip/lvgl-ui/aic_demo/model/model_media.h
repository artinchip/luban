/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#ifndef _AIC_DEMO_MEDIA_H
#define _AIC_DEMO_MEDIA_H

#ifdef __cplusplus
extern "C" {
#endif

#define MEDIA_INFO_MAX                    128

struct media_info {
    char name[128];
    char source_path[128];
    char author[128];
    char introduce[128];
    char cover_path[128];
    int lengths;    /* unit: ms */
};

struct media_list {
    int num;
    int now_pos;
    struct media_info info[MEDIA_INFO_MAX]; /* media_list */
};

struct media_list *media_list_create(void);
int media_list_destroy(struct media_list *list);
int media_list_add_info(struct media_list *list, struct media_info *data);
int media_list_get_now_info(struct media_list *list, struct media_info *data);
int media_list_set_pos(struct media_list *list, char *info_name);
int media_list_set_randomly(struct media_list *list);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
