/*
 * Copyright (c) 2022-2023, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Authors:  Zequan Liang <zequan.liang@artinchip.com>
 */

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "model_media.h"

struct media_list *media_list_create(void)
{
    struct media_list *list = NULL;
    list = (struct media_list *)malloc(sizeof(struct media_list));
    if (list == NULL) {
        return NULL;
    }
    memset(list, 0, sizeof(struct media_list));

    return list;
}

int media_list_destroy(struct media_list *list)
{
    if (list) {
        free(list);
        list = NULL;
        return 0;
    }

    return -1;
}

int media_list_add_info(struct media_list *list, struct media_info *data)
{
    if (list == NULL || data == NULL) {
        return -1;
    }

    int num = list->num;
    if (num >= MEDIA_INFO_MAX) {
        return -1;
    }

    memcpy(&list->info[num], data, sizeof(struct media_info));
    list->num++;

    return 0;
}

int media_list_get_now_info(struct media_list *list, struct media_info *data)
{
    if (list == NULL || data == NULL) {
        return -1;
    }

    memcpy(data, &list->info[list->now_pos], sizeof(struct media_info));
    return 0;
}

int media_list_set_pos(struct media_list *list, char *info_name)
{
    if (list == NULL || info_name == NULL) {
        return -1;
    }

    int num = list->num;
    for(int i = 0; i < num; i++) {
        if (strncmp(info_name, list->info[i].name, strlen(info_name)) == 0) {
            list->now_pos = i;
            return 0;
        }
    }

    return -1;
}

int media_list_set_randomly(struct media_list *list)
{
    if (list == NULL) {
        return -1;
    }

    srand(time(0));

    int num = list->num;
    for(int i = 0; i < num; i++) {
        int j = rand() % i;

        struct media_info tmp;
        memcpy(&tmp, &list->info[i], sizeof(struct media_info));

        /* swap */
        memcpy(&list->info[i], &list->info[j], sizeof(struct media_info));
        memcpy(&list->info[j], &tmp, sizeof(struct media_info));
    }

    return 0;
}
